#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>

#define TEMPLATE_DIR_ENV ".local/template"
#define MAX_TEMPLATES 50
#define TEMPLATE_PATH_SIZE 1024

#define SAFE_SNPRINTF(buf, size, ...)                              \
    do {                                                           \
        int n = snprintf((buf), (size), __VA_ARGS__);              \
        if (n < 0 || n >= (int)(size)) {                           \
            fprintf(stderr, "Buffer size error in %s:%d\n",         \
                    __FILE__, __LINE__);                           \
            exit(EXIT_FAILURE);                                    \
        }                                                          \
    } while (0)

/* -------------------------------------------------------------------------- */
/* Function: print_usage                                                      */
/* Description: Print the usage instructions.                                 */
/* -------------------------------------------------------------------------- */
void print_usage(const char *software) {
    printf("Utilisation : %s [option]\n", software);
    printf("Options :\n");
    printf("  -h                Afficher cette aide et quitter\n");
    printf("  -c NOM:LANGAGE    Créer un projet avec le nom et le langage spécifiés\n");
    printf("  --init            Initialiser les templates en clonant le dépôt GitHub\n");
}


/* -------------------------------------------------------------------------- */
/* Function: init_templates                                                   */
/* Description: add my templates as default                                   */
/* -------------------------------------------------------------------------- */
void init_templates(const char *template_dir_path) {
    struct stat st = {0};
    if (stat(template_dir_path, &st) == -1) {
        if (mkdir(template_dir_path, 0755) != 0) {
            perror("Erreur lors de la création du répertoire de templates");
            exit(EXIT_FAILURE);
        }
    }

    char command[1024];
    snprintf(command, sizeof(command), "git clone https://github.com/chlorat3/template.git \"%s\"", template_dir_path);
    int ret = system(command);
    if (ret != 0) {
        fprintf(stderr, "Erreur lors du clonage du dépôt Git\n");
        exit(EXIT_FAILURE);
    }

    printf("Templates initialisés avec succès dans %s\n", template_dir_path);
}


/* -------------------------------------------------------------------------- */
/* Function: copy_directory                                                   */
/* Description: Copy the source directory to destination using a system call. */
/* -------------------------------------------------------------------------- */
int copy_directory(const char *src, const char *dst) {
    DIR *dir = opendir(src);
    if (!dir) {
        perror("Erreur lors de l'ouverture du répertoire source");
        return -1;
    }

    struct dirent *entry;
    char src_path[1024];
    char dst_path[1024];

    while ((entry = readdir(dir)) != NULL) {
        // Ignorer les entrées spéciales
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;

        SAFE_SNPRINTF(src_path, sizeof(src_path), "%s/%s", src, entry->d_name);
        SAFE_SNPRINTF(dst_path, sizeof(dst_path), "%s/%s", dst, entry->d_name);

        struct stat st;
        if (stat(src_path, &st) == -1) {
            perror("Erreur lors de la lecture des informations du fichier source");
            closedir(dir);
            return -1;
        }

        if (S_ISDIR(st.st_mode)) {
            // Créer le sous-répertoire dans la destination
            if (mkdir(dst_path, 0755) != 0 && errno != EEXIST) {
                perror("Erreur lors de la création du sous-répertoire destination");
                closedir(dir);
                return -1;
            }
            // Appel récursif pour copier le contenu du sous-répertoire
            if (copy_directory(src_path, dst_path) != 0) {
                closedir(dir);
                return -1;
            }
        } else {
            // Copier le fichier
            FILE *src_file = fopen(src_path, "rb");
            if (!src_file) {
                perror("Erreur lors de l'ouverture du fichier source");
                closedir(dir);
                return -1;
            }

            FILE *dst_file = fopen(dst_path, "wb");
            if (!dst_file) {
                perror("Erreur lors de la création du fichier destination");
                fclose(src_file);
                closedir(dir);
                return -1;
            }

            char buffer[8192];
            size_t bytes;
            while ((bytes = fread(buffer, 1, sizeof(buffer), src_file)) > 0) {
                fwrite(buffer, 1, bytes, dst_file);
            }

            fclose(src_file);
            fclose(dst_file);
        }
    }

    closedir(dir);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Function: get_templates                                                    */
/* Description: List directories inside the given template path.              */
/*              Found directory names (i.e. templates) are stored in the array   */
/*              'templates' and the total is set in 'nb_templates'.             */
/* -------------------------------------------------------------------------- */
int get_templates(const char *template_dir_path, char templates[][256], int *nb_templates) {
    DIR *dir = opendir(template_dir_path);
    if (!dir) {
        perror("\033[1;31mError opening templates directory\033[0m");
        return -1;
    }
    struct dirent *entry;
    *nb_templates = 0;
    while ((entry = readdir(dir)) != NULL && *nb_templates < MAX_TEMPLATES) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        /* Build full path to check if the entry is a directory */
        char full_path[512];
        SAFE_SNPRINTF(full_path, sizeof(full_path), "%s/%s", template_dir_path, entry->d_name);
        struct stat st;
        if (stat(full_path, &st) == 0 && S_ISDIR(st.st_mode)) {
            strncpy(templates[*nb_templates], entry->d_name, 255);
            templates[*nb_templates][255] = '\0';
            (*nb_templates)++;
        }
    }
    closedir(dir);
    return 0;
}

#ifndef UNIT_TEST  /* ---------------- APP ---------------- */

int main(int argc, char **argv) {
    /* Build the base template path from HOME environment variable. */
    char template_base_path[TEMPLATE_PATH_SIZE];
    const char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "\033[1;31mError:\033[0m HOME environment variable not found\n");
        return 1;
    }
    SAFE_SNPRINTF(template_base_path, sizeof(template_base_path), "%s/%s", home, TEMPLATE_DIR_ENV);

    /* Non-interactive mode: command line options. */
    if (argc > 1) {
        if (strcmp(argv[1], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[1], "--init") == 0){
            const char *home = getenv("HOME");
            if (home == NULL) {
                fprintf(stderr, "Erreur : variable d'environnement HOME non définie\n");
                return EXIT_FAILURE;
            }

            char template_dir_path[1024];
            SAFE_SNPRINTF(template_dir_path, sizeof(template_dir_path), "%s/.local/template", home);

            init_templates(template_dir_path);
            return EXIT_SUCCESS;

        }else if (strcmp(argv[1], "-c") == 0) {
            if (argc != 3) {
                print_usage(argv[0]);
                return 0;
            }
            char *param = argv[2];
            char project_name[256] = {0};
            char lang[256] = {0};

            /* Simple parser for -c option */
            char *token = strtok(param, ":");
            if (token == NULL) {
                printf("\033[1;31mError:\033[0m invalid format\n");
                print_usage(argv[0]);
                return 0;
            }
            strncpy(project_name, token, sizeof(project_name) - 1);
            token = strtok(NULL, ":");
            if (token == NULL) {
                printf("\033[1;31mError:\033[0m missing language\n");
                print_usage(argv[0]);
                return 1;
            }
            strncpy(lang, token, sizeof(lang) - 1);

            /* Create project directory and change into it. */
            if (mkdir(project_name, 0755) != 0) {
                perror("\033[1;31mError creating project directory\033[0m");
                return 1;
            }
            if (chdir(project_name) != 0) {
                perror("\033[1;31mError entering project directory\033[0m");
                return 1;
            }
            char template_path[TEMPLATE_PATH_SIZE];
            SAFE_SNPRINTF(template_path, sizeof(template_path), "%s/%s", template_base_path, lang);
            if (copy_directory(template_path, ".") != 0) {
                fprintf(stderr, "\033[1;31mError copying files\033[0m\n");
                return 1;
            }
            printf("\033[1;32mProject '%s' successfully created using template '%s'. Happy coding!\033[0m\n", project_name, lang);
            return 0;
        }
    }

    /* ---------------- CLI ---------------- */
    printf("\033[1;34mTEMPLATE MAKER\033[0m\n");

    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == NULL) {
        perror("\033[1;31mError getting current directory\033[0m");
        return 1;
    }
    printf("Creating project in: \"%s\"\n", cwd);

    /* Ask for project name */
    printf("Enter project name: ");
    char project_name[256];
    if (fgets(project_name, sizeof(project_name), stdin) == NULL) {
        fprintf(stderr, "\033[1;31mError reading project name\033[0m\n");
        return 1;
    }
    project_name[strcspn(project_name, "\n")] = 0;  

    if (mkdir(project_name, 0755) != 0) {
        perror("\033[1;31mError creating project directory\033[0m");
        return 1;
    }
    if (chdir(project_name) != 0) {
        perror("\033[1;31mError changing directory\033[0m");
        return 1;
    }

    /* List available templates */
    char templates[MAX_TEMPLATES][256];
    int nb_templates = 0;
    if (get_templates(template_base_path, templates, &nb_templates) != 0 || nb_templates == 0) {
        fprintf(stderr, "\033[1;31mNo available templates in %s\033[0m\n", template_base_path);
        return 1;
    }

    printf("\033[1;34mSelect a template:\033[0m\n");
    for (int i = 0; i < nb_templates; i++) {
        printf("  %d) %s\n", i + 1, templates[i]);
    }
    printf("Choice (number): ");
    char choice_str[4];
    if (fgets(choice_str, sizeof(choice_str), stdin) == NULL) {
        fprintf(stderr, "\033[1;31mError reading choice\033[0m\n");
        return 1;
    }
    int choice = atoi(choice_str);
    if (choice < 1 || choice > nb_templates) {
        printf("\033[1;31mInvalid choice. Exiting.\033[0m\n");
        return 1;
    }
    char selected_template[256];
    strncpy(selected_template, templates[choice - 1], sizeof(selected_template)-1);
    selected_template[sizeof(selected_template)-1] = '\0';

    char template_path[TEMPLATE_PATH_SIZE];
    SAFE_SNPRINTF(template_path, sizeof(template_path), "%s/%s", template_base_path, selected_template);
    if (copy_directory(template_path, ".") != 0) {
        fprintf(stderr, "\033[1;31mError copying template files\033[0m\n");
        return 1;
    }

    printf("\033[1;32mProject '%s' successfully created with template '%s'.\033[0m\n", project_name, selected_template);
    return 0;
}

#else  /* ---------------- TESTS ---------------- */

#include <assert.h>
#include <fcntl.h>

/* -------------------------------------------------------------------------- */
/* Function: create_temp_dir                                                  */
/* Description: Create a temporary directory using mkdtemp, with a name        */
/*              built as "/tmp/" + prefix + "XXXXXX". Ensures the buffer is    */
/*              large enough to hold the full string.                         */
/* -------------------------------------------------------------------------- */
char *create_temp_dir(const char *prefix) {
    /* We use a fixed buffer; the final string is: "/tmp/" + prefix + "XXXXXX" */
    char template_path[512];
    size_t needed = strlen("/tmp/") + strlen(prefix) + 6 + 1;  /* 6 for X's and 1 for '\0' */
    if (needed > sizeof(template_path)) {
        fprintf(stderr, "Prefix too long for temporary directory\n");
        exit(EXIT_FAILURE);
    }
    SAFE_SNPRINTF(template_path, sizeof(template_path), "/tmp/%sXXXXXX", prefix);
    char *dir = mkdtemp(template_path);
    return dir ? strdup(dir) : NULL;
}

/* Test: get_templates should return 0 templates for an empty directory. */
void test_get_templates_empty(void) {
    char *temp_dir = create_temp_dir("empty_");
    assert(temp_dir != NULL);
    char templates[MAX_TEMPLATES][256];
    int nb_templates = 0;
    int ret = get_templates(temp_dir, templates, &nb_templates);
    assert(ret == 0);
    assert(nb_templates == 0);
    char cmd[512];
    SAFE_SNPRINTF(cmd, sizeof(cmd), "rm -rf \"%s\"", temp_dir);
    system(cmd);
    free(temp_dir);
}

/* Test: Create two subdirectories and verify they are detected. */
void test_get_templates_normal(void) {
    char *temp_dir = create_temp_dir("normal_");
    assert(temp_dir != NULL);
    char subdir1[512], subdir2[512];
    SAFE_SNPRINTF(subdir1, sizeof(subdir1), "%s/test1", temp_dir);
    SAFE_SNPRINTF(subdir2, sizeof(subdir2), "%s/test2", temp_dir);
    assert(mkdir(subdir1, 0755) == 0);
    assert(mkdir(subdir2, 0755) == 0);
    char templates[MAX_TEMPLATES][256];
    int nb_templates = 0;
    int ret = get_templates(temp_dir, templates, &nb_templates);
    assert(ret == 0);
    assert(nb_templates >= 2);
    char cmd[512];
    SAFE_SNPRINTF(cmd, sizeof(cmd), "rm -rf \"%s\"", temp_dir);
    system(cmd);
    free(temp_dir);
}

int main(void) {
    printf("Running unit tests...\n");
    test_get_templates_empty();
    printf("test_get_templates_empty passed\n");
    test_get_templates_normal();
    printf("test_get_templates_normal passed\n");
    return 0;
}

#endif  /* UNIT_TEST */

