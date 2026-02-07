/* compile: cc api_demo.c -o api_demo && ./api_demo
 * Creates a demo_tmp/ directory in cwd, cleans up on exit. */
#define SP_PATH_MAX 4096
#define SNAKEPATH_FLUENT
#define SNAKEPATH_IMPLEMENTATION
#include "snakepath.h"
#include <stdio.h>
#include <string.h>

static void section(const char *s) { printf("\n--- %s ---\n\n", s); }

static bool on_walk(struct SpWalkEntry *e) {
    printf("  %s/ (%zu dirs, %zu files)\n",
           sp_str(&e->dirpath), e->dirname_count, e->filename_count);
    return true;
}

int main(void) {
    SpPath tmp;

    SpPath p     = sp_path("/home/user/docs/report.tar.gz");
    SpPath posix = sp_path_f("/etc/nginx/nginx.conf", SP_FLAVOR_POSIX);
    SpPath win   = sp_path_f("C:\\Users\\dev\\file.txt", SP_FLAVOR_WINDOWS);
    SpPath base  = sp_path("/home/user");

    /* Fluent entry: SPF("path"), SPF_P("path"), SPF_W("path"), SPF_PATH(var) */

    /* ── Components ────────────────────────────────────────────── */
    section("Components");

    /* .drive https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.drive */
    printf("  drive:   \"%s\"\n", sp_drive(&p).buf);
    printf("  (fluent) \"%s\"\n", SPF_PATH(p)->drive().buf);

    /* .root https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.root */
    printf("  root:    %s\n", sp_root(&p).buf);
    printf("  (fluent) %s\n", SPF_PATH(p)->root().buf);

    /* .anchor https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.anchor */
    printf("  anchor:  %s\n", sp_anchor(&p).buf);
    printf("  (fluent) %s\n", SPF_PATH(p)->anchor().buf);

    /* .name https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.name */
    printf("  name:    %s\n", sp_name(&p).buf);
    printf("  (fluent) %s\n", SPF_PATH(p)->name().buf);

    /* .stem https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.stem */
    printf("  stem:    %s\n", sp_stem(&p).buf);
    printf("  (fluent) %s\n", SPF_PATH(p)->stem().buf);

    /* .suffix https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffix */
    printf("  suffix:  %s\n", sp_suffix(&p).buf);
    printf("  (fluent) %s\n", SPF_PATH(p)->suffix().buf);

    /* .suffixes https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.suffixes
     * Returns ALL dot-separated segments, not just "real" extensions. */
    SpSuffixes sfx = sp_suffixes(&p);
    printf("  suffixes (%zu):", sfx.count);
    for (size_t i = 0; i < sfx.count; i++)
        printf(" %.*s", (int)sfx.items[i].len, sfx.items[i].data);
    printf("\n");

    /* Windows / POSIX flavor differences */
    printf("  win drive: %s, root: %s\n", sp_drive(&win).buf, sp_root(&win).buf);
    printf("  posix drive: \"%s\", root: %s\n", sp_drive(&posix).buf, sp_root(&posix).buf);

    /* ── Navigation ────────────────────────────────────────────── */
    section("Navigation");

    /* .parent https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parent */
    tmp = sp_parent(&p);
    printf("  parent:  %s\n", sp_str(&tmp));
    tmp = SPF_PATH(p)->parent()->path();
    printf("  (fluent) %s\n", sp_str(&tmp));

    /* .parents https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parents
     * Terminates at "/" for absolute, "." for relative. */
    SpParentsIter pit = sp_parents_begin(&p);
    SpPath par;
    printf("  parents (%zu):\n", sp_parents_count(&p));
    while (sp_parents_next(&pit, &par))
        printf("    %s\n", sp_str(&par));

    /* .parts https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.parts */
    SpPartsIter parts_it = sp_parts_begin(&p);
    SpStr part;
    printf("  parts (%zu):", sp_parts_count(&p));
    while (sp_parts_next(&parts_it, &part))
        printf(" [%.*s]", (int)part.len, part.data);
    printf("\n");

    /* ── Joining ───────────────────────────────────────────────── */
    section("Joining");

    /* .joinpath https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.joinpath */
    tmp = sp_join_one(&base, "docs");
    printf("  join_one: %s\n", sp_str(&tmp));
    tmp = SPF_PATH(base)->join("docs")->path();
    printf("  (fluent)  %s\n", sp_str(&tmp));

    tmp = sp_join(&base, "a", "b", "c"); /* variadic, C only */
    printf("  join:     %s\n", sp_str(&tmp));

    SpPath other = sp_path("extra");
    tmp = sp_joinpath(&base, &other);
    printf("  joinpath: %s\n", sp_str(&tmp));

    /* .with_segments https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_segments */
    const char *segments[] = {"var", "log", "nginx"};
    tmp = sp_with_segments(&base, segments, SP_ARRAY_LEN(segments));
    printf("  with_segments: %s\n", sp_str(&tmp));
    tmp = SPF_PATH(base)->with_segments(segments, SP_ARRAY_LEN(segments))->path();
    printf("  (fluent)      %s\n", sp_str(&tmp));

    /* ── Modification ──────────────────────────────────────────── */
    section("Modification");

    /* .with_name https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_name */
    tmp = sp_with_name(&p, "README.md");
    printf("  with_name:   %s\n", sp_str(&tmp));
    tmp = SPF_PATH(p)->with_name("README.md")->path();
    printf("  (fluent)     %s\n", sp_str(&tmp));

    /* .with_stem https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_stem */
    tmp = sp_with_stem(&p, "archive");
    printf("  with_stem:   %s\n", sp_str(&tmp));
    tmp = SPF_PATH(p)->with_stem("archive")->path();
    printf("  (fluent)     %s\n", sp_str(&tmp));

    /* .with_suffix https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.with_suffix */
    tmp = sp_with_suffix(&p, ".bz2");
    printf("  with_suffix: %s\n", sp_str(&tmp));
    tmp = SPF_PATH(p)->with_suffix(".bz2")->path();
    printf("  (fluent)     %s\n", sp_str(&tmp));

    /* ── Queries ───────────────────────────────────────────────── */
    section("Queries");

    /* .is_absolute https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_absolute */
    printf("  is_absolute:    %d\n", sp_is_absolute(&p));
    printf("  (fluent)        %d\n", SPF_PATH(p)->is_absolute());

    /* .is_relative_to https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.is_relative_to */
    printf("  is_relative_to: %d\n", sp_is_relative_to(&p, &base));
    printf("  (fluent)        %d\n", SPF_PATH(p)->is_relative_to(&base));

    /* .relative_to https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.relative_to */
    tmp = sp_relative_to(&p, &base);
    printf("  relative_to:    %s\n", sp_str(&tmp));
    tmp = SPF_PATH(p)->relative_to(&base)->path();
    printf("  (fluent)        %s\n", sp_str(&tmp));

    /* .relative_to(walk_up=True) */
    SpPath sibling = sp_path("/home/other/file.txt");
    tmp = sp_relative_to_walk_up(&sibling, &base);
    printf("  walk_up:        %s\n", sp_str(&tmp));

    /* .match https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.match */
    printf("  match *.gz:     %d\n", SP_MATCH(&p, "*.gz"));

    printf("  is_reserved:    %d\n", sp_is_reserved(&p));

    /* ── Comparison ────────────────────────────────────────────── */
    section("Comparison");

    SpPath a = sp_path("/foo/bar"), b = sp_path("/foo/bar");

    /* __eq__ / __ne__ */
    printf("  eq:   %d\n", sp_path_eq(&a, &b));
    printf("  ne:   %d\n", sp_path_ne(&a, &b));
    printf("  (fluent eq) %d\n", SPF_PATH(a)->eq(&b));
    printf("  (fluent ne) %d\n", SPF_PATH(a)->ne(&b));

    printf("  cmp:  %d\n", sp_path_cmp(&a, &b));
    printf("  hash: %lu\n", sp_path_hash(&a));

    /* ── String Conversion ─────────────────────────────────────── */
    section("String Conversion");

    /* .as_posix https://docs.python.org/3/library/pathlib.html#pathlib.PurePath.as_posix */
    char posix_buf[SP_PATH_MAX];
    sp_as_posix(&win, posix_buf, sizeof(posix_buf));
    printf("  as_posix: %s\n", posix_buf);

    /* .as_uri https://docs.python.org/3/library/pathlib.html#pathlib.Path.as_uri */
    char uri_buf[SP_PATH_MAX];
    sp_as_uri(&p, uri_buf, sizeof(uri_buf));
    printf("  as_uri:   %s\n", uri_buf);

    SpPath converted = sp_path_convert("C:\\Users\\dev", SP_FLAVOR_WINDOWS, SP_FLAVOR_POSIX);
    printf("  convert:  %s\n", sp_str(&converted));

    /* ── Chaining ──────────────────────────────────────────────── */
    section("Chaining");

    SpPath built = SPF_P("/home/user")
        ->join("projects")->join("snakepath")
        ->with_name("libsnakepath.so")
        ->path();
    printf("  %s\n", sp_str(&built));

    SpTerm fname = SPF("/etc/nginx/nginx.conf")->parent()->name();
    printf("  %s\n", fname.buf);

    /* ── Filesystem ────────────────────────────────────────────── */
    section("Filesystem");

    /* Path.cwd https://docs.python.org/3/library/pathlib.html#pathlib.Path.cwd */
    SpPath cwd = sp_cwd(SP_FLAVOR_NATIVE);
    printf("  cwd:        %s\n", sp_str(&cwd));

    /* Path.home https://docs.python.org/3/library/pathlib.html#pathlib.Path.home */
    SpPath home = sp_home(SP_FLAVOR_NATIVE);
    printf("  home:       %s\n", sp_str(&home));

    /* .expanduser https://docs.python.org/3/library/pathlib.html#pathlib.Path.expanduser */
    SpPath tilde = sp_path("~/docs");
    tmp = sp_expanduser(&tilde);
    printf("  expanduser: %s\n", sp_str(&tmp));
    tmp = SPF_PATH(tilde)->expanduser()->path();
    printf("  (fluent)    %s\n", sp_str(&tmp));

    /* .absolute https://docs.python.org/3/library/pathlib.html#pathlib.Path.absolute */
    SpPath dot = sp_path(".");
    tmp = sp_absolute(&dot);
    printf("  absolute:   %s\n", sp_str(&tmp));

    /* .resolve https://docs.python.org/3/library/pathlib.html#pathlib.Path.resolve */
    tmp = sp_resolve(&dot, false);
    printf("  resolve:    %s\n", sp_str(&tmp));

    /* ── Type Checks ───────────────────────────────────────────── */
    section("Type Checks");

    /* .exists https://docs.python.org/3/library/pathlib.html#pathlib.Path.exists */
    printf("  exists:     %d\n", sp_exists(&cwd));
    printf("  (fluent)    %d\n", SPF_PATH(cwd)->exists());

    /* .is_dir https://docs.python.org/3/library/pathlib.html#pathlib.Path.is_dir */
    printf("  is_dir:     %d\n", sp_is_dir(&cwd));

    /* .is_file https://docs.python.org/3/library/pathlib.html#pathlib.Path.is_file */
    printf("  is_file:    %d\n", sp_is_file(&cwd));

    /* .is_symlink https://docs.python.org/3/library/pathlib.html#pathlib.Path.is_symlink */
    printf("  is_symlink: %d\n", sp_is_symlink(&cwd));
    /* Also: sp_is_block_device, sp_is_char_device, sp_is_fifo,
             sp_is_socket, sp_is_mount, sp_is_junction */

    /* .stat https://docs.python.org/3/library/pathlib.html#pathlib.Path.stat */
    /* sp_stat follows symlinks; sp_lstat does not */
    SpStatResult st = sp_stat(&cwd);
    printf("  stat size:  %lld\n", st.sp_size);

    /* ── File I/O ──────────────────────────────────────────────── */
    section("File I/O");

    SpPath tmpdir  = sp_join_one(&cwd, "demo_tmp");
    SpPath subdir  = sp_join_one(&tmpdir, "sub");
    SpPath tmpfile = sp_join_one(&tmpdir, "hello.txt");

    /* .mkdir https://docs.python.org/3/library/pathlib.html#pathlib.Path.mkdir */
    sp_mkdir(&subdir, 0755, true, true);
    printf("  mkdir:   demo_tmp/sub/\n");

    /* .touch https://docs.python.org/3/library/pathlib.html#pathlib.Path.touch */
    sp_touch(&tmpfile, 0644, true);

    /* .write_bytes https://docs.python.org/3/library/pathlib.html#pathlib.Path.write_bytes */
    const char *msg = "Hello, snakepath!";
    SpIOResult wr = sp_write_file(&tmpfile, msg, strlen(msg));
    printf("  write:   %zu bytes (err=%d)\n", wr.bytes, wr.error);

    /* .read_bytes https://docs.python.org/3/library/pathlib.html#pathlib.Path.read_bytes */
    char buf[256];
    SpIOResult rd = sp_read_file(&tmpfile, buf, sizeof(buf));
    buf[rd.bytes] = '\0';
    printf("  read:    \"%s\"\n", buf);

    /* .rename https://docs.python.org/3/library/pathlib.html#pathlib.Path.rename */
    SpPath renamed = sp_join_one(&tmpdir, "renamed.txt");
    sp_rename(&tmpfile, &renamed);
    printf("  rename:  hello.txt -> renamed.txt\n");

    /* .chmod https://docs.python.org/3/library/pathlib.html#pathlib.Path.chmod */
    sp_chmod(&renamed, 0600);

    /* .owner https://docs.python.org/3/library/pathlib.html#pathlib.Path.owner */
    printf("  owner:   %s\n", sp_owner(&renamed).buf);
    printf("  (fluent) %s\n", SPF_PATH(renamed)->owner().buf);

    /* .group https://docs.python.org/3/library/pathlib.html#pathlib.Path.group */
    printf("  group:   %s\n", sp_group(&renamed).buf);

    /* ── Symlinks ──────────────────────────────────────────────── */
    section("Symlinks");

    SpPath link = sp_join_one(&tmpdir, "link.txt");

    /* .symlink_to https://docs.python.org/3/library/pathlib.html#pathlib.Path.symlink_to */
    sp_symlink_to(&link, &renamed, false);

    /* .readlink https://docs.python.org/3/library/pathlib.html#pathlib.Path.readlink */
    tmp = sp_readlink(&link);
    printf("  readlink: %s\n", sp_str(&tmp));

    /* .samefile https://docs.python.org/3/library/pathlib.html#pathlib.Path.samefile */
    printf("  samefile: %d\n", sp_samefile(&link, &renamed));
    printf("  (fluent)  %d\n", SPF_PATH(link)->samefile(&renamed));

    /* ── Glob & Iterdir ────────────────────────────────────────── */
    section("Glob & Iterdir");

    /* .iterdir https://docs.python.org/3/library/pathlib.html#pathlib.Path.iterdir */
    SP_ITERDIR_FOREACH(&tmpdir, entry)
        printf("  iterdir: %s\n", sp_str(&entry));

    /* .glob https://docs.python.org/3/library/pathlib.html#pathlib.Path.glob */
    SP_GLOB_FOREACH(&tmpdir, "*.txt", match)
        printf("  glob:    %s\n", sp_str(&match));

    /* .rglob https://docs.python.org/3/library/pathlib.html#pathlib.Path.rglob */
    SP_RGLOB_FOREACH(&tmpdir, "*.txt", match)
        printf("  rglob:   %s\n", sp_str(&match));

    /* ── Walk ──────────────────────────────────────────────────── */
    section("Walk");

    /* .walk https://docs.python.org/3/library/pathlib.html#pathlib.Path.walk
     * Callback-based, allocation-free, unlimited depth via stack recursion. */
    sp_walk(&tmpdir, true, false, on_walk, NULL, NULL);

    /* ── Error Handling ────────────────────────────────────────── */
    section("Error Handling");

    /* Operations that fail return SpPath checked with sp_path_is_error(). */
    SpPath no_name = sp_path("/");
    SpPath err = sp_with_name(&no_name, "x");
    if (sp_path_is_error(&err))
        printf("  with_name(\"/\", \"x\"): %s\n", sp_error_str(sp_path_error_code(&err)));

    sp_unlink(&link, false);
    sp_unlink(&renamed, false);
    sp_rmdir(&subdir);
    sp_rmdir(&tmpdir);

    return 0;
}
