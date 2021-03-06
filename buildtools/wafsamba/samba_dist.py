# customised version of 'waf dist' for Samba tools
# uses git ls-files to get file lists

import os, sys, tarfile
from waflib import Utils, Scripting, Logs, Options
from waflib.Configure import conf
from samba_utils import get_string
from waflib import Context

dist_dirs = None
dist_files = None
dist_blacklist = ""
dist_archive = None

class Dist(Context.Context):
    # TODO remove
    cmd = 'dist'
    fun = 'dist'
    def execute(self):
        Context.g_module.dist()

class DistCheck(Scripting.DistCheck):
    fun = 'distcheck'
    cmd = 'distcheck'
    def execute(self):
        Options.options.distcheck_args = ''
        if Context.g_module.distcheck is Scripting.distcheck:
            # default
            Context.g_module.distcheck(self)
        else:
            Context.g_module.distcheck()
        Context.g_module.dist()
        self.check()
    def get_arch_name(self):
        global dist_archive
        return dist_archive
    def make_distcheck_cmd(self, tmpdir):
        waf = os.path.abspath(sys.argv[0])
        return [sys.executable, waf, 'configure', 'build', 'install', 'uninstall', '--destdir=' + tmpdir]

def add_symlink(tar, fname, abspath, basedir):
    '''handle symlinks to directories that may move during packaging'''
    if not os.path.islink(abspath):
        return False
    tinfo = tar.gettarinfo(name=abspath, arcname=fname)
    tgt = os.readlink(abspath)

    if dist_dirs:
        # we need to find the target relative to the main directory
        # this is here to cope with symlinks into the buildtools
        # directory from within the standalone libraries in Samba. For example,
        # a symlink to ../../builtools/scripts/autogen-waf.sh needs
        # to be rewritten as a symlink to buildtools/scripts/autogen-waf.sh
        # when the tarball for talloc is built

        # the filename without the appname-version
        rel_fname = '/'.join(fname.split('/')[1:])

        # join this with the symlink target
        tgt_full = os.path.join(os.path.dirname(rel_fname), tgt)

        # join with the base directory
        tgt_base = os.path.normpath(os.path.join(basedir, tgt_full))

        # see if this is inside one of our dist_dirs
        for dir in dist_dirs.split():
            if dir.find(':') != -1:
                destdir=dir.split(':')[1]
                dir=dir.split(':')[0]
            else:
                destdir = '.'
            if dir == basedir:
                # internal links don't get rewritten
                continue
            if dir == tgt_base[0:len(dir)] and tgt_base[len(dir)] == '/':
                new_tgt = destdir + tgt_base[len(dir):]
                tinfo.linkname = new_tgt
                break

    tinfo.uid   = 0
    tinfo.gid   = 0
    tinfo.uname = 'root'
    tinfo.gname = 'root'
    tar.addfile(tinfo)
    return True

def add_tarfile(tar, fname, abspath, basedir):
    '''add a file to the tarball'''
    if add_symlink(tar, fname, abspath, basedir):
        return
    try:
        tinfo = tar.gettarinfo(name=abspath, arcname=fname)
    except OSError:
        Logs.error('Unable to find file %s - missing from git checkout?' % abspath)
        sys.exit(1)
    tinfo.uid   = 0
    tinfo.gid   = 0
    tinfo.uname = 'root'
    tinfo.gname = 'root'
    fh = open(abspath, "rb")
    tar.addfile(tinfo, fileobj=fh)
    fh.close()


def vcs_dir_contents(path):
    """Return the versioned files under a path.

    :return: List of paths relative to path
    """
    repo = path
    while repo != "/":
        if os.path.exists(os.path.join(repo, ".git")):
            ls_files_cmd = [ 'git', 'ls-files', '--full-name',
                             os.path.relpath(path, repo) ]
            cwd = None
            env = dict(os.environ)
            env["GIT_DIR"] = os.path.join(repo, ".git")
            break
        repo = os.path.dirname(repo)
    if repo == "/":
        raise Exception("unsupported or no vcs for %s" % path)
    return get_string(Utils.cmd_output(ls_files_cmd, cwd=cwd, env=env)).split('\n')


def dist(appname='', version=''):

    def add_files_to_tarball(tar, srcdir, srcsubdir, dstdir, dstsubdir, blacklist, files):
        if blacklist is None:
            blacklist = []
        for f in files:
            abspath = os.path.join(srcdir, f)

            if srcsubdir != '.':
                f = f[len(srcsubdir)+1:]

            # Remove files in the blacklist
            if f in blacklist:
                continue
            blacklisted = False
            # Remove directories in the blacklist
            for d in blacklist:
                if f.startswith(d):
                    blacklisted = True
            if blacklisted:
                continue
            if os.path.isdir(abspath) and not os.path.islink(abspath):
                continue
            if dstsubdir != '.':
                f = dstsubdir + '/' + f
            fname = dstdir + '/' + f
            add_tarfile(tar, fname, abspath, srcsubdir)


    def list_directory_files(path):
        curdir = os.getcwd()
        os.chdir(srcdir)
        out_files = []
        for root, dirs, files in os.walk(path):
            for f in files:
                out_files.append(os.path.join(root, f))
        os.chdir(curdir)
        return out_files


    if not isinstance(appname, str) or not appname:
        # this copes with a mismatch in the calling arguments for dist()
        appname = Context.g_module.APPNAME
        version = Context.g_module.VERSION
    if not version:
        version = Context.g_module.VERSION

    srcdir = os.path.normpath(
                os.path.join(os.path.dirname(Context.g_module.root_path),
                    Context.g_module.top))

    if not dist_dirs:
        Logs.error('You must use samba_dist.DIST_DIRS() to set which directories to package')
        sys.exit(1)

    dist_base = '%s-%s' % (appname, version)

    if Options.options.SIGN_RELEASE:
        dist_name = '%s.tar' % (dist_base)
        tar = tarfile.open(dist_name, 'w')
    else:
        dist_name = '%s.tar.gz' % (dist_base)
        tar = tarfile.open(dist_name, 'w:gz')

    blacklist = dist_blacklist.split()

    for dir in dist_dirs.split():
        if dir.find(':') != -1:
            destdir=dir.split(':')[1]
            dir=dir.split(':')[0]
        else:
            destdir = '.'
        absdir = os.path.join(srcdir, dir)
        try:
            files = vcs_dir_contents(absdir)
        except Exception as e:
            Logs.error('unable to get contents of %s: %s' % (absdir, e))
            sys.exit(1)
        add_files_to_tarball(tar, srcdir, dir, dist_base, destdir, blacklist, files)

    if dist_files:
        for file in dist_files.split():
            if file.find(':') != -1:
                destfile = file.split(':')[1]
                file = file.split(':')[0]
            else:
                destfile = file

            absfile = os.path.join(srcdir, file)

            if os.path.isdir(absfile) and not os.path.islink(absfile):
                destdir = destfile
                dir = file
                files = list_directory_files(dir)
                add_files_to_tarball(tar, srcdir, dir, dist_base, destdir, blacklist, files)
            else:
                fname = dist_base + '/' + destfile
                add_tarfile(tar, fname, absfile, destfile)

    tar.close()

    if Options.options.SIGN_RELEASE:
        import gzip
        try:
            os.unlink(dist_name + '.asc')
        except OSError:
            pass

        cmd = "gpg --detach-sign --armor " + dist_name
        os.system(cmd)
        uncompressed_tar = open(dist_name, 'rb')
        compressed_tar = gzip.open(dist_name + '.gz', 'wb')
        while 1:
            buffer = uncompressed_tar.read(1048576)
            if buffer:
                compressed_tar.write(buffer)
            else:
                break
        uncompressed_tar.close()
        compressed_tar.close()
        os.unlink(dist_name)
        Logs.info('Created %s.gz %s.asc' % (dist_name, dist_name))
        dist_name = dist_name + '.gz'
    else:
        Logs.info('Created %s' % dist_name)

    # TODO use the ctx object instead
    global dist_archive
    dist_archive = dist_name
    return dist_name


@conf
def DIST_DIRS(dirs):
    '''set the directories to package, relative to top srcdir'''
    global dist_dirs
    if not dist_dirs:
        dist_dirs = dirs

@conf
def DIST_FILES(files, extend=False):
    '''set additional files for packaging, relative to top srcdir'''
    global dist_files
    if not dist_files:
        dist_files = files
    elif extend:
        dist_files = dist_files + " " + files

@conf
def DIST_BLACKLIST(blacklist):
    '''set the files to exclude from packaging, relative to top srcdir'''
    global dist_blacklist
    if not dist_blacklist:
        dist_blacklist = blacklist

Scripting.dist = dist
