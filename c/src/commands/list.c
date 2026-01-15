/*
 * List command implementation.
 * Lists all connected MCC 134 boards.
 */

#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <daqhats/daqhats.h>

#include "commands/list.h"
#include "hardware.h"
#include "utils.h"

#include "cJSON.h"

/* Command: list - List all connected MCC 134 boards */
int cmd_list(int argc, char **argv) {
    int json_output = 0;
    
    /* Parse options */
    static struct option long_options[] = {
        {"json", no_argument, 0, 'j'},
        {0, 0, 0, 0}
    };
    
    int opt;
    while ((opt = getopt_long(argc, argv, "j", long_options, NULL)) != -1) {
        switch (opt) {
            case 'j':
                json_output = 1;
                break;
            default:
                fprintf(stderr, "Usage: thermo-cli list [--json]\n");
                return 1;
        }
    }
    
    struct HatInfo *boards = NULL;
    int count = 0;
    
    int result = thermo_list_boards(&boards, &count);
    if (result != THERMO_SUCCESS) {
        fprintf(stderr, "Error listing boards\n");
        return 1;
    }
    
    if (json_output) {
        cJSON *root = cJSON_CreateObject();
        cJSON *boards_array = cJSON_CreateArray();
        
        for (int i = 0; i < count; i++) {
            cJSON *board = cJSON_CreateObject();
            cJSON_AddNumberToObject(board, "address", boards[i].address);
            cJSON_AddStringToObject(board, "id", "MCC 134");
            cJSON_AddStringToObject(board, "name", boards[i].product_name);
            cJSON_AddItemToArray(boards_array, board);
        }
        
        cJSON_AddItemToObject(root, "boards", boards_array);
        char *json_str = cJSON_Print(root);
        printf("%s\n", json_str);
        free(json_str);
        cJSON_Delete(root);
    } else {
        if (count == 0) {
            printf("No MCC 134 boards detected.\n");
        } else {
            Table *table = table_create(3);
            table_set_header(table, 0, "Address");
            table_set_header(table, 1, "ID");
            table_set_header(table, 2, "Name");
            
            for (int i = 0; i < count; i++) {
                char addr_str[16];
                snprintf(addr_str, sizeof(addr_str), "%d", boards[i].address);
                
                char *row[3] = {addr_str, "MCC 134", boards[i].product_name};
                table_add_row(table, row);
            }
            
            table_print(table, "Connected MCC 134 Boards");
            table_free(table);
        }
    }
    
    if (boards) free(boards);
    return 0;
}
