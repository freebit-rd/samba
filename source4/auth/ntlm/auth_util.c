/* 
   Unix SMB/CIFS implementation.
   Authentication utility functions
   Copyright (C) Andrew Tridgell 1992-1998
   Copyright (C) Andrew Bartlett 2001
   Copyright (C) Jeremy Allison 2000-2001
   Copyright (C) Rafal Szczesniak 2002
   Copyright (C) Stefan Metzmacher 2005

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "includes.h"
#include "auth/auth.h"
#include "libcli/auth/libcli_auth.h"
#include "param/param.h"
#include "auth/ntlm/auth_proto.h"
#include "librpc/gen_ndr/drsuapi.h"
#include "dsdb/samdb/samdb.h"
#include "lib/crypto/gnutls_helpers.h"

#undef DBGC_CLASS
#define DBGC_CLASS DBGC_AUTH

/****************************************************************************
 Create an auth_usersupplied_data structure after appropriate mapping.
****************************************************************************/

NTSTATUS encrypt_user_info(TALLOC_CTX *mem_ctx, struct auth4_context *auth_context, 
			   enum auth_password_state to_state,
			   const struct auth_usersupplied_info *user_info_in,
			   const struct auth_usersupplied_info **user_info_encrypted)
{
	int rc;
	NTSTATUS nt_status;
	struct auth_usersupplied_info *user_info_temp;
	switch (to_state) {
	case AUTH_PASSWORD_RESPONSE:
		switch (user_info_in->password_state) {
		case AUTH_PASSWORD_PLAIN:
		{
			const struct auth_usersupplied_info *user_info_temp2;
			nt_status = encrypt_user_info(mem_ctx, auth_context, 
						      AUTH_PASSWORD_HASH, 
						      user_info_in, &user_info_temp2);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
			user_info_in = user_info_temp2;

			FALL_THROUGH;
		}
		case AUTH_PASSWORD_HASH:
		{
			uint8_t chal[8];
			DATA_BLOB chall_blob;
			user_info_temp = talloc_zero(mem_ctx, struct auth_usersupplied_info);
			if (!user_info_temp) {
				return NT_STATUS_NO_MEMORY;
			}
			if (!talloc_reference(user_info_temp, user_info_in)) {
				return NT_STATUS_NO_MEMORY;
			}
			*user_info_temp = *user_info_in;
			user_info_temp->password_state = to_state;
			
			nt_status = auth_get_challenge(auth_context, chal);
			if (!NT_STATUS_IS_OK(nt_status)) {
				return nt_status;
			}
			
			chall_blob = data_blob_talloc(mem_ctx, chal, 8);
			if (lpcfg_client_ntlmv2_auth(auth_context->lp_ctx)) {
				DATA_BLOB names_blob = NTLMv2_generate_names_blob(mem_ctx,  lpcfg_netbios_name(auth_context->lp_ctx), lpcfg_workgroup(auth_context->lp_ctx));
				DATA_BLOB lmv2_response, ntlmv2_response, lmv2_session_key, ntlmv2_session_key;
				
				if (!SMBNTLMv2encrypt_hash(user_info_temp,
							   user_info_in->client.account_name, 
							   user_info_in->client.domain_name, 
							   user_info_in->password.hash.nt->hash,
							   &chall_blob,
							   NULL, /* server_timestamp */
							   &names_blob,
							   &lmv2_response, &ntlmv2_response, 
							   &lmv2_session_key, &ntlmv2_session_key)) {
					data_blob_free(&names_blob);
					return NT_STATUS_NO_MEMORY;
				}
				data_blob_free(&names_blob);
				user_info_temp->password.response.lanman = lmv2_response;
				user_info_temp->password.response.nt = ntlmv2_response;
				
				data_blob_free(&lmv2_session_key);
				data_blob_free(&ntlmv2_session_key);
			} else {
				DATA_BLOB blob = data_blob_talloc(mem_ctx, NULL, 24);
				rc = SMBOWFencrypt(user_info_in->password.hash.nt->hash, chal, blob.data);
				if (rc != 0) {
					return gnutls_error_to_ntstatus(rc, NT_STATUS_ACCESS_DISABLED_BY_POLICY_OTHER);
				}
				user_info_temp->password.response.nt = blob;
				if (lpcfg_client_lanman_auth(auth_context->lp_ctx) && user_info_in->password.hash.lanman) {
					DATA_BLOB lm_blob = data_blob_talloc(mem_ctx, NULL, 24);
					rc = SMBOWFencrypt(user_info_in->password.hash.lanman->hash, chal, blob.data);
					if (rc != 0) {
						return gnutls_error_to_ntstatus(rc, NT_STATUS_ACCESS_DISABLED_BY_POLICY_OTHER);
					}
					user_info_temp->password.response.lanman = lm_blob;
				} else {
					/* if not sending the LM password, send the NT password twice */
					user_info_temp->password.response.lanman = user_info_temp->password.response.nt;
				}
			}

			user_info_in = user_info_temp;

			FALL_THROUGH;
		}
		case AUTH_PASSWORD_RESPONSE:
			*user_info_encrypted = user_info_in;
		}
		break;
	case AUTH_PASSWORD_HASH:
	{	
		switch (user_info_in->password_state) {
		case AUTH_PASSWORD_PLAIN:
		{
			struct samr_Password lanman;
			struct samr_Password nt;
			
			user_info_temp = talloc_zero(mem_ctx, struct auth_usersupplied_info);
			if (!user_info_temp) {
				return NT_STATUS_NO_MEMORY;
			}
			if (!talloc_reference(user_info_temp, user_info_in)) {
				return NT_STATUS_NO_MEMORY;
			}
			*user_info_temp = *user_info_in;
			user_info_temp->password_state = to_state;
			
			if (E_deshash(user_info_in->password.plaintext, lanman.hash)) {
				user_info_temp->password.hash.lanman = talloc(user_info_temp,
									      struct samr_Password);
				*user_info_temp->password.hash.lanman = lanman;
			} else {
				user_info_temp->password.hash.lanman = NULL;
			}
			
			E_md4hash(user_info_in->password.plaintext, nt.hash);
			user_info_temp->password.hash.nt = talloc(user_info_temp,
								   struct samr_Password);
			*user_info_temp->password.hash.nt = nt;
			
			user_info_in = user_info_temp;

			FALL_THROUGH;
		}
		case AUTH_PASSWORD_HASH:
			*user_info_encrypted = user_info_in;
			break;
		default:
			return NT_STATUS_INVALID_PARAMETER;
			break;
		}
		break;
	}
	default:
		return NT_STATUS_INVALID_PARAMETER;
	}

	return NT_STATUS_OK;
}
