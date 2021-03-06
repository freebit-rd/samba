/*
   Copyright (C) Bjoern Jacke <bjacke@samba.org> 2022

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

#ifndef __VFS_AIXACL_H__
#define __VFS_AIXACL_H__

SMB_ACL_T aixacl_sys_acl_get_fd(vfs_handle_struct *handle,
                                files_struct *fsp,
                                SMB_ACL_TYPE_T type,
                                TALLOC_CTX *mem_ctx);

int aixacl_sys_acl_set_fd(vfs_handle_struct *handle,
				 files_struct *fsp,
				 SMB_ACL_TYPE_T type,
				 SMB_ACL_T acl_d);

int aixacl_sys_acl_delete_def_fd(vfs_handle_struct *handle,
                                 files_struct *fsp);

#endif
