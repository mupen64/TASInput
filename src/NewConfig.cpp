#include "NewConfig.h"
#include "helpers/io_helpers.h"

#define CONFIG_PATH "tasinput.cfg"

constexpr t_config default_config;
t_config new_config;


void save_config()
{
    printf("Saving config...\n");
    FILE* f = fopen(CONFIG_PATH, "wb");
    if (!f)
    {
        printf("Can't save config\n");
        return;
    }
    fwrite(&new_config, sizeof(t_config), 1, f);
    fclose(f);
}

void load_config()
{
    printf("Loading config...\n");

    auto buffer = read_file_buffer(CONFIG_PATH);
    t_config loaded_config;

    if (buffer.empty() || buffer.size() != sizeof(t_config))
    {
        // Failed, reset to default
        printf("No config found, using default\n");
        loaded_config = default_config;
    } else
    {
        uint8_t* ptr = buffer.data();
        memread(&ptr, &loaded_config, sizeof(t_config));
    }
    
    if (loaded_config.version < default_config.version)
    {
        // Outdated version, reset to default
        printf("Outdated config version, using default\n");
        loaded_config = default_config;
    }

    new_config = loaded_config;
}
