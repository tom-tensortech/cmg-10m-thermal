/*
 * Main entry point for thermo-cli.
 * Handles argument parsing and command dispatch using argp.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <argp.h>

#include "commands/list.h"
#include "commands/get.h"
#include "commands/set.h"
#include "commands/fuse.h"
#include "commands/init_config.h"

const char *argp_program_version = "thermo-cli 1.0.0";
const char *argp_program_bug_address = "<support@example.com>";

/* Program documentation */
static char doc[] = 
    "thermo-cli -- MCC 134 Thermocouple Interface and Data Fuser\n\n"
    "COMMANDS:\n"
    "  list             List all connected MCC 134 boards\n"
    "  get              Read data from a specific channel\n"
    "  set              Configure channel parameters\n"
    "  fuse             Fuse thermal data into cmg-cli output\n"
    "  init-config      Generate an example configuration file\n";

/* Argument documentation */
static char args_doc[] = "COMMAND [ARGS...]";

/* Command function type */
typedef int (*command_func)(int argc, char **argv);

/* Command structure */
typedef struct {
    const char *name;
    const char *help;
    command_func func;
} Command;

/* Available commands */
static Command commands[] = {
    {"list", "List all connected MCC 134 boards", cmd_list},
    {"get", "Read data from a specific channel", cmd_get},
    {"set", "Configure channel parameters", cmd_set},
    {"fuse", "Fuse thermal data into cmg-cli output", cmd_fuse},
    {"init-config", "Generate example configuration file", cmd_init_config},
    {NULL, NULL, NULL}
};

/* Options */
static struct argp_option options[] = {
    {0}
};

/* Parse a single option */
static error_t parse_opt(int key, char *arg, struct argp_state *state) {
    switch (key) {
        case ARGP_KEY_ARG:
            /* We handle arguments in main() */
            return ARGP_ERR_UNKNOWN;
        default:
            return ARGP_ERR_UNKNOWN;
    }
    return 0;
}

/* Argp parser */
static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

/* Print help for a specific command */
static void print_command_help(const char *cmd_name) {
    if (strcmp(cmd_name, "list") == 0) {
        printf("Usage: thermo-cli list [OPTIONS]\n\n");
        printf("List all connected MCC 134 boards.\n\n");
        printf("Options:\n");
        printf("  -j, --json          Output as JSON\n");
    } else if (strcmp(cmd_name, "get") == 0) {
        printf("Usage: thermo-cli get [OPTIONS]\n\n");
        printf("Read data from a specific channel.\n\n");
        printf("Options:\n");
        printf("  -a, --address NUM        Board address (0-7) [default: 0]\n");
        printf("  -c, --channel NUM        Channel index (0-3) [default: 0]\n");
        printf("  -t, --tc-type TYPE       Thermocouple type (K,J,T,E,R,S,B,N) [default: K]\n");
        printf("  -s, --serial             Get serial number\n");
        printf("  -D, --cali-date          Get calibration date\n");
        printf("  -C, --cali-coeffs        Get calibration coefficients\n");
        printf("  -T, --temp               Get temperature (default if nothing else specified)\n");
        printf("  -A, --adc                Get raw ADC voltage\n");
        printf("  -J, --cjc                Get CJC temperature\n");
        printf("  -i, --update-interval    Get update interval\n");
        printf("  -S, --stream HZ          Stream readings at specified frequency (Hz)\n");
        printf("  -j, --json               Output as JSON\n");
    } else if (strcmp(cmd_name, "set") == 0) {
        printf("Usage: thermo-cli set [OPTIONS]\n\n");
        printf("Configure channel parameters.\n\n");
        printf("Options:\n");
        printf("  -a, --address NUM           Board address (0-7) [default: 0]\n");
        printf("  -c, --channel NUM           Channel index (0-3) [default: 0]\n");
        printf("  -S, --cali-slope VALUE      Set calibration slope\n");
        printf("  -O, --cali-offset VALUE     Set calibration offset\n");
        printf("  -i, --update-interval NUM   Set update interval in seconds\n");
    } else if (strcmp(cmd_name, "fuse") == 0) {
        printf("Usage: thermo-cli fuse [OPTIONS] -- [cmg-cli arguments...]\n\n");
        printf("Fuse thermal data into 'cmg-cli get' command output.\n\n");
        printf("Options:\n");
        printf("  -C, --config FILE      Path to YAML/JSON config file\n");
        printf("  -a, --address NUM      Single mode: Board address\n");
        printf("  -c, --channel NUM      Single mode: Channel index\n");
        printf("  -k, --key NAME         Single mode: JSON key to inject [default: TEMP_FUSED]\n\n");
        printf("Examples:\n");
        printf("  thermo-cli fuse --address 0 --channel 1 --key MY_TEMP -- --power --json\n");
        printf("  thermo-cli fuse --config config.yaml -- --actuator --stream 5 --json\n");
    } else if (strcmp(cmd_name, "init-config") == 0) {
        printf("Usage: thermo-cli init-config [OPTIONS]\n\n");
        printf("Generate an example configuration file.\n\n");
        printf("Options:\n");
        printf("  -o, --output FILE      Output file path [default: thermo_config.yaml]\n");
    } else {
        printf("Unknown command: %s\n", cmd_name);
        printf("Run 'thermo-cli --help' for available commands.\n");
    }
}

/* Main entry point */
int main(int argc, char **argv) {
    /* Check for help flags first */
    if (argc > 1 && (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0)) {
        argp_help(&argp, stdout, ARGP_HELP_STD_HELP, "thermo-cli");
        return 0;
    }
    
    /* Check for version flag */
    if (argc > 1 && (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0)) {
        printf("%s\n", argp_program_version);
        return 0;
    }
    
    /* Require at least one argument (the command) */
    if (argc < 2) {
        fprintf(stderr, "Error: No command specified\n\n");
        argp_help(&argp, stderr, ARGP_HELP_STD_USAGE, "thermo-cli");
        return 1;
    }
    
    /* Get command name */
    const char *cmd_name = argv[1];
    
    /* Check for command-specific help */
    if (argc > 2 && (strcmp(argv[2], "--help") == 0 || strcmp(argv[2], "-h") == 0)) {
        print_command_help(cmd_name);
        return 0;
    }
    
    /* Find and execute command */
    for (int i = 0; commands[i].name != NULL; i++) {
        if (strcmp(cmd_name, commands[i].name) == 0) {
            /* Shift arguments so command sees its own argv[0] */
            char *cmd_argv[argc];
            cmd_argv[0] = argv[0]; /* Keep program name */
            for (int j = 2; j < argc; j++) {
                cmd_argv[j - 1] = argv[j];
            }
            int cmd_argc = argc - 1;
            
            /* Reset optind for command's getopt parsing */
            optind = 1;
            
            return commands[i].func(cmd_argc, cmd_argv);
        }
    }
    
    /* Command not found */
    fprintf(stderr, "Error: Unknown command '%s'\n\n", cmd_name);
    fprintf(stderr, "Available commands:\n");
    for (int i = 0; commands[i].name != NULL; i++) {
        fprintf(stderr, "  %-15s %s\n", commands[i].name, commands[i].help);
    }
    fprintf(stderr, "\nRun 'thermo-cli COMMAND --help' for command-specific help.\n");
    
    return 1;
}
