#include <stdio.h>
#include <iostream>
#include <string>
#include <vector>
#include <cstring>
#include <cstdlib>
#include "common.h"
#include <yaml.h>

using namespace std;

vector<string> readyaml(cl_ops *ops, std::string fname)
{
    vector<string> signaldevices;

    FILE *fh = fopen(fname.data(), "r");
    if (!fh) {
        cerr << "readyaml: cannot open " << fname << endl;
        return signaldevices;
    }

    yaml_parser_t parser;
    yaml_event_t  event;

    if (!yaml_parser_initialize(&parser)) {
        cerr << "readyaml: failed to initialize parser" << endl;
        fclose(fh);
        return signaldevices;
    }
    yaml_parser_set_input_file(&parser, fh);

    int  mapping_depth  = 0;
    int  seq_depth      = 0;
    bool in_params      = false;  // inside the parameters sequence
    bool saw_params_key = false;  // just saw "parameters" key at root level
    bool root_val_next  = false;  // root mapping: next scalar is a value
    bool item_val_next  = false;  // parameter item mapping: next scalar is a value
    string param_id;              // id of current parameter block
    string last_key;              // last key seen in current parameter block

    bool done = false;
    while (!done) {
        if (!yaml_parser_parse(&parser, &event)) {
            cerr << "readyaml: parse error: " << parser.problem << endl;
            break;
        }

        switch (event.type) {
        case YAML_STREAM_END_EVENT:
        case YAML_DOCUMENT_END_EVENT:
            done = true;
            break;

        case YAML_MAPPING_START_EVENT:
            mapping_depth++;
            if (mapping_depth == 2 && in_params) {
                param_id      = "";
                last_key      = "";
                item_val_next = false;
            }
            break;

        case YAML_MAPPING_END_EVENT:
            mapping_depth--;
            break;

        case YAML_SEQUENCE_START_EVENT:
            seq_depth++;
            if (saw_params_key) {
                in_params      = true;
                saw_params_key = false;
                root_val_next  = false;  // sequence consumed the value slot
            }
            break;

        case YAML_SEQUENCE_END_EVENT:
            seq_depth--;
            if (seq_depth == 0)
                in_params = false;
            break;

        case YAML_SCALAR_EVENT: {
            const char *val = (const char *)event.data.scalar.value;

            if (!in_params && mapping_depth == 1) {
                if (!root_val_next) {
                    if (strcmp(val, "parameters") == 0)
                        saw_params_key = true;
                    root_val_next = true;
                } else {
                    root_val_next = false;
                }
            } else if (in_params && mapping_depth == 2) {
                if (!item_val_next) {
                    last_key      = string(val);
                    item_val_next = true;
                } else {
                    if (last_key == "id") {
                        param_id = string(val);
                    } else if (last_key == "default") {
                        if (param_id == "num_samples")
                            ops->blocksize = atoi(val) * 2;
                        else if (param_id == "freq")
                            ops->fc = (uint32_t)atof(val);
                        else if (param_id == "samplerate")
                            ops->fs = (uint32_t)atof(val);
                    } else if (param_id == "devices") {
                        if (last_key == "referencedevice") {
                            ops->refname = string(val);
                            signaldevices.push_back(string(val));
                        } else if (last_key.find("signaldevice") == 0) {
                            signaldevices.push_back(string(val));
                        }
                    }
                    item_val_next = false;
                }
            }
            break;
        }

        default:
            break;
        }

        yaml_event_delete(&event);
    }

    yaml_parser_delete(&parser);
    fclose(fh);
    return signaldevices;
}
