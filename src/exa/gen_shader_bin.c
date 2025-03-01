/*
 * Copyright (c) Dmitry Osipenko
 * Copyright (c) Erik Faye-Lund
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <locale.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>

#include "asm.h"

static char *vs_path;
static char *fs_path;
static char *lnk_path;
static char *fp_name;
static char *vp_name;
static char *lp_name;
static char *out_name;

static int parse_command_line(int argc, char *argv[])
{
    int ret;
    int c;

    do {
        struct option long_options[] =
        {
            {"vs",      required_argument, NULL, 0},
            {"fs",      required_argument, NULL, 0},
            {"lnk",     required_argument, NULL, 0},
            {"fpname",  required_argument, NULL, 0},
            {"vpname",  required_argument, NULL, 0},
            {"lpname",  required_argument, NULL, 0},
            {"out",     required_argument, NULL, 0},
            { /* Sentinel */ }
        };
        int option_index = 0;

        c = getopt_long(argc, argv, "", long_options, &option_index);

        switch (c) {
        case 0:
            switch (option_index) {
            case 0:
                vs_path = optarg;
                break;
            case 1:
                fs_path = optarg;
                break;
            case 2:
                lnk_path = optarg;
                break;
            case 3:
                fp_name = optarg;
                break;
            case 4:
                vp_name = optarg;
                break;
            case 5:
                lp_name = optarg;
                break;
            case 6:
                out_name = optarg;
                break;
            default:
                return 0;
            }
            break;
        case -1:
            break;
        default:
            fprintf(stderr, "Invalid arguments\n\n");
            return 0;
        }
    } while (c != -1);

    return 1;
}

static char *read_file(const char *path)
{
    struct stat sb;
    char *data;
    int fd;

    fd = open(path, O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "Failed to open %s: %s\n", path, strerror(errno));
        abort();
    }

    if (fstat(fd, &sb) == -1) {
        fprintf(stderr, "Failed to get stat %s: %s\n",
                path, strerror(errno));
        abort();
    }

    data = calloc(1, sb.st_size + 1);
    if (!data) {
        fprintf(stderr, "Failed to allocate %ju: %s\n",
                (uintmax_t)sb.st_size, path);
        abort();
    }

    if (read(fd, data, sb.st_size) == -1) {
        fprintf(stderr, "Failed to read %s: %s\n", path, strerror(errno));
        abort();
    }

    close(fd);

    return data;
}

int main(int argc, char *argv[])
{
    uint32_t in_mask = 0, out_mask = 0;
    char *asm_txt;
    unsigned int i;
    FILE *out;
    int err;

    /* float decimal point is locale-dependent */
    setlocale(LC_ALL, "C");

    if (!parse_command_line(argc, argv))
        return 1;

    /* parse vertex asm */
    asm_txt = read_file(vs_path);
    if (!asm_txt)
        return 1;

    vertex_asm_scan_string(asm_txt);
    err = vertex_asmparse();
    if (err)
        return err;

    vertex_asmlex_destroy();
    free(asm_txt);

    /* parse linker asm */
    asm_txt = read_file(lnk_path);
    if (!asm_txt)
        return 1;

    linker_asm_scan_string(asm_txt);
    err = linker_asmparse();
    if (err)
        return err;

    linker_asmlex_destroy();
    free(asm_txt);

    /* parse fragment asm */
    asm_txt = read_file(fs_path);
    if (!asm_txt)
        return 1;

    fragment_asm_scan_string(asm_txt);
    err = fragment_asmparse();
    if (err)
        return err;

    fragment_asmlex_destroy();
    free(asm_txt);

    out = fopen(out_name, "w");
    if (!out) {
        fprintf(stderr, "Failed to open %s: %s\n", out_name, strerror(errno));
        return 1;
    }

    fprintf(out, "/* Autogenerated file */\n\n");
    fprintf(out, "#include \"gpu/gr3d.h\"\n\n");

    fprintf(out, "#ifndef fp_%s_def\n", fp_name);
    fprintf(out, "#define fp_%s_def\n", fp_name);
    fprintf(out, "static const uint32_t fs_%s_words[] = {\n", fp_name);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x541, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_pseq_instructions[i].data);

    fprintf(out, "    HOST1X_OPCODE_IMM(0x500, 0x0),\n");

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x601, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_mfu_sched[i].data);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x604, %d),\n",
            asm_mfu_instructions_nb * 2);
    for (i = 0; i < asm_mfu_instructions_nb; i++) {
        fprintf(out, "    0x%08X,\n", asm_mfu_instructions[i].part1);
        fprintf(out, "    0x%08X,\n", asm_mfu_instructions[i].part0);
    }

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x701, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_tex_instructions[i].data);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x801, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_alu_sched[i].data);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x804, %d),\n",
            asm_alu_instructions_nb * 8);
    for (i = 0; i < asm_alu_instructions_nb; i++) {
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part1);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part0);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part3);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part2);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part5);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part4);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part7);
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].part6);
    }

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x806, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_alu_instructions[i].complement);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x901, %d),\n",
            asm_fs_instructions_nb);
    for (i = 0; i < asm_fs_instructions_nb; i++)
        fprintf(out, "    0x%08X,\n", asm_dw_instructions[i].data);

    fprintf(out, "};\n");
    fprintf(out, "#endif\n\n");

    fprintf(out, "#ifndef vp_%s_def\n", vp_name);
    fprintf(out, "#define vp_%s_def\n", vp_name);
    fprintf(out, "static const uint32_t vs_%s_words[] = {\n", vp_name);

    fprintf(out, "    HOST1X_OPCODE_NONINCR(0x206, %d),\n",
           asm_vs_instructions_nb * 4);

    for (i = 0; i < asm_vs_instructions_nb; i++) {
        fprintf(out, "    0x%08X,\n", asm_vs_instructions[i].part3);
        fprintf(out, "    0x%08X,\n", asm_vs_instructions[i].part2);
        fprintf(out, "    0x%08X,\n", asm_vs_instructions[i].part1);
        fprintf(out, "    0x%08X,\n", asm_vs_instructions[i].part0);
    }

    fprintf(out, "};\n");
    fprintf(out, "#endif\n\n");

    for (i = 0; i < 16; i++) {
        if (asm_vs_attributes[i].used)
            in_mask |= 1 << i;

        if (asm_vs_exports[i].used)
            out_mask |= 1 << i;
    }

    fprintf(out, "#ifndef lp_%s_def\n", lp_name);
    fprintf(out, "#define lp_%s_def\n", lp_name);
    fprintf(out, "static const uint32_t lnk_%s_words[] = {\n", lp_name);

    fprintf(out, "    HOST1X_OPCODE_INCR(0x300, %d),\n",
           asm_linker_instructions_nb * 2);

    for (i = 0; i < asm_linker_instructions_nb; i++) {
        fprintf(out, "    0x%08X,\n", asm_linker_instructions[i].first);
        fprintf(out, "    0x%08X,\n", asm_linker_instructions[i].latter);
    }

    fprintf(out, "};\n");
    fprintf(out, "#endif\n\n");

    fprintf(out, "#ifndef prog_%s_def\n", fp_name);
    fprintf(out, "#define prog_%s_def\n", fp_name);
    fprintf(out, "static const struct shader_program prog_%s = {\n", fp_name);
    fprintf(out, "    .vs_prog_words = vs_%s_words,\n", vp_name);
    fprintf(out, "    .vs_prog_words_nb = TEGRA_ARRAY_SIZE(vs_%s_words),\n", vp_name);
    fprintf(out, "    .vs_attrs_in_mask = %u,\n", in_mask);
    fprintf(out, "    .vs_attrs_out_mask = %u,\n", out_mask);
    fprintf(out, "\n");
    fprintf(out, "    .fs_prog_words = fs_%s_words,\n", fp_name);
    fprintf(out, "    .fs_prog_words_nb = TEGRA_ARRAY_SIZE(fs_%s_words),\n", fp_name);
    fprintf(out, "    .fs_alu_buf_size = %u,\n", asm_alu_buffer_size);
    fprintf(out, "    .fs_pseq_to_dw = %u,\n", asm_pseq_to_dw_exec_nb);
    fprintf(out, "    .fs_pseq_inst_nb = %u,\n", asm_fs_instructions_nb);
    fprintf(out, "\n");
    fprintf(out, "    .linker_words = lnk_%s_words,\n", lp_name);
    fprintf(out, "    .linker_words_nb = TEGRA_ARRAY_SIZE(lnk_%s_words),\n", lp_name);
    fprintf(out, "    .linker_inst_nb = %u,\n", asm_linker_instructions_nb);
    fprintf(out, "    .used_tram_rows_nb = %u,\n", asm_linker_used_tram_rows_nb);
    fprintf(out, "\n");
    fprintf(out, "    .name = \"%s\",\n", fp_name);
    fprintf(out, "};\n");
    fprintf(out, "#endif\n");

    return 0;
}
