#include "config/io.h"

#include "config/error.h"

#include <stdlib.h>

#define OSD_CONFIG_MAX_SIZE_BYTES (1024L * 1024L)

// Reads the full config file into memory with a hard size limit
bool osd_config_read_file_text(const char *path, char **out_json_text, FILE *err_stream) {
    FILE *config_file = NULL;
    char *json_text = NULL;
    long file_size = 0L;
    size_t read_size = 0U;

    if (path == NULL || out_json_text == NULL || err_stream == NULL) {
        return false;
    }

    config_file = fopen(path, "rb");
    if (config_file == NULL) {
        osd_config_write_error_value_message(err_stream, "Failed to open config file: ", path, "\n");
        return false;
    }

    if (fseek(config_file, 0L, SEEK_END) != 0) {
        osd_config_write_error_value_message(err_stream, "Failed to seek config file: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    file_size = ftell(config_file);
    if (file_size < 0L || file_size > OSD_CONFIG_MAX_SIZE_BYTES) {
        osd_config_write_error_value_message(err_stream, "Invalid config file size for: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    if (fseek(config_file, 0L, SEEK_SET) != 0) {
        osd_config_write_error_value_message(err_stream, "Failed to read config file: ", path, "\n");
        (void)fclose(config_file);
        return false;
    }

    json_text = calloc((size_t)file_size + 1U, sizeof(char));
    if (json_text == NULL) {
        osd_config_write_error_text(err_stream, "Failed to allocate memory for config file\n");
        (void)fclose(config_file);
        return false;
    }

    read_size = fread(json_text, 1U, (size_t)file_size, config_file);
    (void)fclose(config_file);

    if (read_size != (size_t)file_size) {
        osd_config_write_error_value_message(err_stream, "Failed to fully read config file: ", path, "\n");
        free(json_text);
        return false;
    }

    *out_json_text = json_text;
    return true;
}
