#include "syntaxes.h"
#include "pattern_cache.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "analyzers.h"
#include <ctype.h>
#include "logger.h"

LanguageType languageType(const char* filename) {
    if (!filename) return LANG_RUST; // Default to first language
    
    const char* ext = strrchr(filename, '.');
    if (!ext) {
        logr(DEBUG, "[Syntaxes] Skipping file without extension: %s", filename);
        return LANG_RUST; // Default to first language
    }
    
    ext++; // Skip the dot
    
    // Convert extension to lowercase for comparison
    char ext_lower[32] = {0};
    size_t i;
    for (i = 0; i < sizeof(ext_lower) - 1 && ext[i]; i++) {
        ext_lower[i] = tolower(ext[i]);
    }
    
    // Match extensions to language types
    if (strcmp(ext_lower, "rs") == 0) {
        return LANG_RUST;
    } else if (strcmp(ext_lower, "c") == 0 || 
               strcmp(ext_lower, "h") == 0 || 
               strcmp(ext_lower, "cpp") == 0 || 
               strcmp(ext_lower, "hpp") == 0) {
        return LANG_C;
    } else if (strcmp(ext_lower, "js") == 0 || 
               strcmp(ext_lower, "jsx") == 0 || 
               strcmp(ext_lower, "ts") == 0 || 
               strcmp(ext_lower, "tsx") == 0) {
        return LANG_JAVASCRIPT;
    } else if (strcmp(ext_lower, "go") == 0) {
        return LANG_GO;
    } else if (strcmp(ext_lower, "py") == 0) {
        return LANG_PYTHON;
    } else if (strcmp(ext_lower, "java") == 0) {
        return LANG_JAVA;
    } else if (strcmp(ext_lower, "php") == 0) {
        return LANG_PHP;
    } else if (strcmp(ext_lower, "rb") == 0) {
        return LANG_RUBY;
    }
    
    logr(DEBUG, "[Syntaxes] Unsupported file extension: %s", ext_lower);
    return LANG_RUST; // Default to first language instead of -1
}


// Analyze a file based on the given configuration
ExtractedDependency* analyze_file(const char* file_path, AnalysisConfig* config) {
    if (!file_path || !config) return NULL;

    // Read file content
    FILE* file = fopen(file_path, "r");
    if (!file) return NULL;

    // Get file size
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);

    // Allocate memory for content
    char* content = malloc(size + 1);
    if (!content) {
        fclose(file);
        return NULL;
    }

    // Read file content
    size_t read_size = fread(content, 1, size, file);
    content[read_size] = '\0';
    fclose(file);

    // Create dependency structure
    ExtractedDependency* dep = malloc(sizeof(ExtractedDependency));
    if (!dep) {
        free(content);
        return NULL;
    }
    
    // Initialize dependency
    dep->file_path = strdup(file_path);
    dep->language = languageType(file_path);
    
    // Get grammar
    const LanguageGrammar* grammar = languageGrammars(dep->language);
    if (grammar) {
        // Update this line to include file_path
        dep->structures = analyze_structure(content, file_path, grammar);
        dep->methods = analyzeMethod(file_path, content, grammar);
        dep->modules = analyzeModule(content, grammar);
    }

    free(content);
    return dep;
}

// Create a dependency graph from extracted dependencies
DependencyGraph* create_dependency_graph(ExtractedDependency** deps, int dep_count) {
    if (!deps || dep_count <= 0) return NULL;

    DependencyGraph* graph = malloc(sizeof(DependencyGraph));
    graph->relationships = malloc(sizeof(Relationship*) * dep_count * MAX_DEPENDENCIES);
    graph->relationship_count = 0;
    graph->current_layer = LAYER_MODULE;

    for (int i = 0; i < dep_count; i++) {
        ExtractedDependency* dep = deps[i];
        if (!dep) continue;

        // Process module-level relationships
        if (dep->module_name) {
            Relationship* rel = malloc(sizeof(Relationship));
            rel->from = strdup(dep->file_path);
            rel->to = strdup(dep->module_name);
            rel->relationship_type = strdup("imports");
            rel->layer = LAYER_MODULE;
            graph->relationships[graph->relationship_count++] = rel;
        }

        // Process structure-level relationships
        if (dep->structures) {
            Structure* curr_struct = dep->structures;
            while (curr_struct) {
                if (curr_struct->dependencies) {
                    Relationship* rel = malloc(sizeof(Relationship));
                    rel->from = strdup(curr_struct->name);
                    rel->to = strdup(curr_struct->dependencies);
                    rel->relationship_type = strdup("inherits");
                    rel->layer = LAYER_STRUCT;
                    graph->relationships[graph->relationship_count++] = rel;
                }
                curr_struct = curr_struct->next;
            }
        }

        // Process method-level relationships
        if (dep->methods) {
            Method* curr_method = dep->methods;
            while (curr_method) {
                if (curr_method->dependencies) {
                    Relationship* rel = malloc(sizeof(Relationship));
                    rel->from = strdup(curr_method->name);
                    rel->to = strdup(curr_method->dependencies);
                    rel->relationship_type = strdup("calls");
                    rel->layer = LAYER_METHOD;
                    graph->relationships[graph->relationship_count++] = rel;
                }
                curr_method = curr_method->next;
            }
        }
    }

    return graph;
}

// Export the dependency graph to a specific format
void exportGraph(DependencyGraph* graph, const char* format, const char* output_path) {
    if (!graph || !format || !output_path) return;

    FILE* output = fopen(output_path, "w");
    if (!output) return;

    if (strcmp(format, "dot") == 0) {
        // Export as GraphViz DOT format
        fprintf(output, "digraph Dependencies {\n");
        for (int i = 0; i < graph->relationship_count; i++) {
            Relationship* rel = graph->relationships[i];
            fprintf(output, "  \"%s\" -> \"%s\" [label=\"%s\"];\n",
                    rel->from, rel->to, rel->relationship_type);
        }
        fprintf(output, "}\n");
    } else if (strcmp(format, "json") == 0) {
        // Export as JSON format
        fprintf(output, "{\n  \"relationships\": [\n");
        for (int i = 0; i < graph->relationship_count; i++) {
            Relationship* rel = graph->relationships[i];
            fprintf(output, "    {\n");
            fprintf(output, "      \"from\": \"%s\",\n", rel->from);
            fprintf(output, "      \"to\": \"%s\",\n", rel->to);
            fprintf(output, "      \"type\": \"%s\",\n", rel->relationship_type);
            fprintf(output, "      \"layer\": %d\n", rel->layer);
            fprintf(output, "    }%s\n", i < graph->relationship_count - 1 ? "," : "");
        }
        fprintf(output, "  ]\n}\n");
    }

    fclose(output);
}

const char* languageName(LanguageType type) {
    switch (type) {
        case LANG_RUST: return "Rust";
        case LANG_C: return "C/C++";
        case LANG_JAVASCRIPT: return "JavaScript";
        case LANG_GO: return "Go";
        case LANG_PYTHON: return "Python";
        case LANG_JAVA: return "Java";
        case LANG_PHP: return "PHP";
        case LANG_RUBY: return "Ruby";
        default: return "Unknown";
    }
}

void freeStructures(Structure* structs) {
    while (structs) {
        Structure* next = structs->next;
        
        // Free the structure name
        free(structs->name);
        
        // Free methods
        if (structs->methods) {
            for (int i = 0; i < structs->method_count; i++) {
                free(structs->methods[i].name);
                free(structs->methods[i].return_type);
                
                // Free parameters
                for (int j = 0; j < structs->methods[i].param_count; j++) {
                    free(structs->methods[i].parameters[j].name);
                    free(structs->methods[i].parameters[j].type);
                    free(structs->methods[i].parameters[j].default_value);
                }
                
                free(structs->methods[i].dependencies);
            }
            free(structs->methods);
        }
        
        // Free implemented traits
        if (structs->implemented_traits) {
            for (int i = 0; i < structs->trait_count; i++) {
                free(structs->implemented_traits[i]);
            }
            free(structs->implemented_traits);
        }
        
        // Free dependencies
        free(structs->dependencies);
        
        // Free the structure itself
        free(structs);
        structs = next;
    }
}

void freeMethods(Method* methods) {
    while (methods) {
        Method* next = methods->next;
        
        // Free method name and return type
        free(methods->name);
        free(methods->return_type);
        
        // Free parameters
        for (int i = 0; i < methods->param_count; i++) {
            free(methods->parameters[i].name);
            free(methods->parameters[i].type);
            free(methods->parameters[i].default_value);
        }
        
        // Free dependencies
        free(methods->dependencies);
        
        // Free the method itself
        free(methods);
        methods = next;
    }
}