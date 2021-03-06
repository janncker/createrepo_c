/* createrepo_c - Library of routines for manipulation with repodata
 * Copyright (C) 2012  Tomas Mlcoch
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

#include <string.h>
#include <assert.h>
#include "cmd_parser.h"
#include "error.h"
#include "compression_wrapper.h"
#include "misc.h"


#define DEFAULT_CHANGELOG_LIMIT         10
#define DEFAULT_CHECKSUM                "sha256"
#define DEFAULT_WORKERS                 5
#define DEFAULT_UNIQUE_MD_FILENAMES     TRUE


struct CmdOptions _cmd_options = {
        .changelog_limit     = DEFAULT_CHANGELOG_LIMIT,
        .checksum            = NULL,
        .workers             = DEFAULT_WORKERS,
        .unique_md_filenames = DEFAULT_UNIQUE_MD_FILENAMES,
        .checksum_type       = CR_CHECKSUM_SHA256,
        .compression_type    = CR_CW_UNKNOWN_COMPRESSION
    };



// Command line params

static GOptionEntry cmd_entries[] =
{
    { "version", 'V', 0, G_OPTION_ARG_NONE, &(_cmd_options.version),
      "Show program's version number and exit.", NULL},
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &(_cmd_options.quiet),
      "Run quietly.", NULL },
    { "verbose", 'v', 0, G_OPTION_ARG_NONE, &(_cmd_options.verbose),
      "Run verbosely.", NULL },
    { "excludes", 'x', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.excludes),
      "File globs to exclude, can be specified multiple times.", "<packages>" },
    { "basedir", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.basedir),
      "Basedir for path to directories.", "<basedir>" },
    { "baseurl", 'u', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.location_base),
      "Optional base URL location for all files.", "<URL>" },
    { "groupfile", 'g', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.groupfile),
      "Path to groupfile to include in metadata.",
      "GROUPFILE" },
    { "checksum", 's', 0, G_OPTION_ARG_STRING, &(_cmd_options.checksum),
      "Choose the checksum type used in repomd.xml and for packages in the "
      "metadata. The default is now \"sha256\".", "<checksum_type>" },
    { "pretty", 'p', 0, G_OPTION_ARG_NONE, &(_cmd_options.pretty),
      "Make sure all xml generated is formatted (default)", NULL },
    { "database", 'd', 0, G_OPTION_ARG_NONE, &(_cmd_options.database),
      "Generate sqlite databases for use with yum.", NULL },
    { "no-database", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.no_database),
      "Do not generate sqlite databases in the repository.", NULL },
    { "update", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.update),
      "If metadata already exists in the outputdir and an rpm is unchanged "
      "(based on file size and mtime) since the metadata was generated, reuse "
      "the existing metadata rather than recalculating it. In the case of a "
      "large repository with only a few new or modified rpms "
      "this can significantly reduce I/O and processing time.", NULL },
    { "update-md-path", 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.update_md_paths),
      "Use the existing repodata for --update from this path.", NULL },
    { "skip-stat", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_stat),
      "Skip the stat() call on a --update, assumes if the filename is the same "
      "then the file is still the same (only use this if you're fairly "
      "trusting or gullible).", NULL },
    { "pkglist", 'i', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.pkglist),
      "Specify a text file which contains the complete list of files to "
      "include in the repository from the set found in the directory. File "
      "format is one package per line, no wildcards or globs.", "<filename>" },
    { "includepkg", 'n', 0, G_OPTION_ARG_FILENAME_ARRAY, &(_cmd_options.includepkg),
      "Specify pkgs to include on the command line. Takes urls as well as local paths.",
      "<packages>" },
    { "outputdir", 'o', 0, G_OPTION_ARG_FILENAME, &(_cmd_options.outputdir),
      "Optional output directory.", "<URL>" },
    { "skip-symlinks", 'S', 0, G_OPTION_ARG_NONE, &(_cmd_options.skip_symlinks),
      "Ignore symlinks of packages.", NULL},
    { "changelog-limit", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.changelog_limit),
      "Only import the last N changelog entries, from each rpm, into the metadata.",
      "<number>" },
    { "unique-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.unique_md_filenames),
      "Include the file's checksum in the metadata filename, helps HTTP caching (default).",
      NULL },
    { "simple-md-filenames", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.simple_md_filenames),
      "Do not include the file's checksum in the metadata filename.", NULL },
    { "retain-old-md", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.retain_old),
      "Keep around the latest (by timestamp) N copies of the old repodata.", NULL },
    { "distro", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.distro_tags),
      "Distro tag and optional cpeid: --distro'cpeid,textname'.", "DISTRO" },
    { "content", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.content_tags),
      "Tags for the content in the repository.", "CONTENT_TAGS" },
    { "repo", 0, 0, G_OPTION_ARG_STRING_ARRAY, &(_cmd_options.repo_tags),
      "Tags to describe the repository itself.", "REPO_TAGS" },
    { "revision", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.revision),
      "User-specified revision for this repository.", "REVISION" },
    { "read-pkgs-list", 0, 0, G_OPTION_ARG_FILENAME, &(_cmd_options.read_pkgs_list),
      "Output the paths to the pkgs actually read useful with --update.",
      "READ_PKGS_LIST" },
    { "workers", 0, 0, G_OPTION_ARG_INT, &(_cmd_options.workers),
      "Number of workers to spawn to read rpms.", NULL },
    { "xz", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.xz_compression),
      "Use xz for repodata compression.", NULL },
    { "compress-type", 0, 0, G_OPTION_ARG_STRING, &(_cmd_options.compress_type),
      "Which compression type to use.", "<compress_type>" },
    { "keep-all-metadata", 0, 0, G_OPTION_ARG_NONE, &(_cmd_options.keep_all_metadata),
      "Keep groupfile and updateinfo from source repo during update.", NULL },
    { NULL, 0, 0, G_OPTION_ARG_NONE, NULL, NULL, NULL },
};



struct CmdOptions *parse_arguments(int *argc, char ***argv, GError **err)
{
    gboolean ret;
    GOptionContext *context;

    assert(!err || *err == NULL);

    context = g_option_context_new("- program that creates a repomd (xml-based"
                                   " rpm metadata) repository from a set of"
                                   " rpms.");
    g_option_context_add_main_entries(context, cmd_entries, NULL);

    ret = g_option_context_parse(context, argc, argv, err);
    g_option_context_free(context);

    if (!ret)
        return NULL;

    return &(_cmd_options);
}



gboolean
check_arguments(struct CmdOptions *options,
                const char *input_dir,
                GError **err)
{
    assert(!err || *err == NULL);

    // Check outputdir
    if (options->outputdir && !g_file_test(options->outputdir, G_FILE_TEST_EXISTS|G_FILE_TEST_IS_DIR)) {
        g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                    "Specified outputdir \"%s\" doesn't exists",
                    options->outputdir);
        return FALSE;
    }

    // Check workers
    if ((options->workers < 1) || (options->workers > 100)) {
        g_warning("Wrong number of workers - Using 5 workers.");
        options->workers = DEFAULT_WORKERS;
    }

    // Check changelog_limit
    if ((options->changelog_limit < 0) || (options->changelog_limit > 100)) {
        g_warning("Wrong changelog limit \"%d\" - Using 10", options->changelog_limit);
        options->changelog_limit = DEFAULT_CHANGELOG_LIMIT;
    }

    // Check simple filenames
    if (options->simple_md_filenames) {
        options->unique_md_filenames = FALSE;
    }

    // Check and set checksum type
    if (options->checksum) {
        cr_ChecksumType type;
        type = cr_checksum_type(options->checksum);
        if (type == CR_CHECKSUM_UNKNOWN) {
            g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                        "Unknown/Unsupported checksum type \"%s\"",
                        options->checksum);
            return FALSE;
        }
        options->checksum_type = type;
    }

    // Check and set compression type
    if (options->compress_type) {
        GString *compress_str = g_string_ascii_down(g_string_new(options->compress_type));
        if (!strcmp(compress_str->str, "gz")) {
            options->compression_type = CR_CW_GZ_COMPRESSION;
        } else if (!strcmp(compress_str->str, "bz2")) {
            options->compression_type = CR_CW_BZ2_COMPRESSION;
        } else if (!strcmp(compress_str->str, "xz")) {
            options->compression_type = CR_CW_XZ_COMPRESSION;
        } else {
            g_string_free(compress_str, TRUE);
            g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                        "Unknown/Unsupported compression type \"%s\"",
                        options->compress_type);
            return FALSE;
        }
        g_string_free(compress_str, TRUE);
    }

    int x;

    // Process exclude glob masks
    x = 0;
    while (options->excludes && options->excludes[x] != NULL) {
        GPatternSpec *pattern = g_pattern_spec_new(options->excludes[x]);
        options->exclude_masks = g_slist_prepend(options->exclude_masks,
                                                 (gpointer) pattern);
        x++;
    }

    // Process includepkgs
    x = 0;
    while (options->includepkg && options->includepkg[x] != NULL) {
        options->include_pkgs = g_slist_prepend(options->include_pkgs,
                                  (gpointer) g_strdup(options->includepkg[x]));
        x++;
    }

    // Check groupfile
    options->groupfile_fullpath = NULL;
    if (options->groupfile) {
        gboolean remote = FALSE;

        if (g_str_has_prefix(options->groupfile, "/")) {
            // Absolute local path
            options->groupfile_fullpath = g_strdup(options->groupfile);
        } else if (strstr(options->groupfile, "://")) {
            // Remote groupfile
            remote = TRUE;
            options->groupfile_fullpath = g_strdup(options->groupfile);
        } else {
            // Relative path (from intput_dir)
            options->groupfile_fullpath = g_strconcat(input_dir,
                                                      options->groupfile,
                                                      NULL);
        }

        if (!remote && !g_file_test(options->groupfile_fullpath, G_FILE_TEST_IS_REGULAR)) {
            g_set_error(err, CR_CMD_ERROR, CRE_BADARG,
                        "groupfile %s doesn't exists",
                        options->groupfile_fullpath);
            return FALSE;
        }
    }

    // Process pkglist file
    if (options->pkglist) {
        if (!g_file_test(options->pkglist, G_FILE_TEST_IS_REGULAR)) {
            g_warning("pkglist file \"%s\" doesn't exists", options->pkglist);
        } else {
            char *content = NULL;
            GError *tmp_err = NULL;
            if (!g_file_get_contents(options->pkglist, &content, NULL, &tmp_err)) {
                g_warning("Error while reading pkglist file: %s", tmp_err->message);
                g_error_free(tmp_err);
                g_free(content);
            } else {
                x = 0;
                char **pkgs = g_strsplit(content, "\n", 0);
                while (pkgs && pkgs[x] != NULL) {
                    if (strlen(pkgs[x])) {
                        options->include_pkgs = g_slist_prepend(options->include_pkgs,
                                                 (gpointer) g_strdup(pkgs[x]));
                    }
                    x++;
                }

                g_strfreev(pkgs);
                g_free(content);
            }
        }
    }

    // Process update_md_paths
    if (options->update_md_paths && !options->update)
        g_warning("Usage of --update-md-path without --update has no effect!");

    x = 0;
    while (options->update_md_paths && options->update_md_paths[x] != NULL) {
        char *path = options->update_md_paths[x];
        options->l_update_md_paths = g_slist_prepend(options->l_update_md_paths,
                                                     (gpointer) path);
        x++;
    }

    // Check keep-all-metadata
    if (options->keep_all_metadata && !options->update) {
        g_warning("--keep-all-metadata has no effect (--update is not used)");
    }

    // Process --distro tags
    x = 0;
    while (options->distro_tags && options->distro_tags[x]) {
        if (!strchr(options->distro_tags[x], ',')) {
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    NULL);
            options->distro_values = g_slist_append(options->distro_values,
                                        g_strdup(options->distro_tags[x]));
            x++;
            continue;
        }

        gchar **items = g_strsplit(options->distro_tags[x++], ",", 2);
        if (!items) continue;
        if (!items[0] || !items[1] || items[1][0] == '\0') {
            g_strfreev(items);
            continue;
        }

        if (items[0][0] != '\0')
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    g_strdup(items[0]));
        else
            options->distro_cpeids = g_slist_append(options->distro_cpeids,
                                                    NULL);
        options->distro_values = g_slist_append(options->distro_values,
                                                g_strdup(items[1]));
        g_strfreev(items);
    }

    return TRUE;
}



void
free_options(struct CmdOptions *options)
{
    g_free(options->basedir);
    g_free(options->location_base);
    g_free(options->outputdir);
    g_free(options->pkglist);
    g_free(options->checksum);
    g_free(options->compress_type);
    g_free(options->groupfile);
    g_free(options->groupfile_fullpath);
    g_free(options->revision);

    g_strfreev(options->excludes);
    g_strfreev(options->includepkg);
    g_strfreev(options->distro_tags);
    g_strfreev(options->content_tags);
    g_strfreev(options->repo_tags);

    cr_slist_free_full(options->include_pkgs, g_free);
    cr_slist_free_full(options->exclude_masks,
                      (GDestroyNotify) g_pattern_spec_free);
    cr_slist_free_full(options->l_update_md_paths, g_free);
    cr_slist_free_full(options->distro_cpeids, g_free);
    cr_slist_free_full(options->distro_values, g_free);
}
