#include "Combo.h"

std::vector<Combo*> find_combos(const char* path)
{
    FILE* f = fopen(path, "rb");

    if (!f)
    {
        printf("Can't find combos.cmb\n");
        return {};
    }

    std::vector<Combo*> combos;
    char current = fgetc(f);
    
    while (current != -1)
    {
        auto combo = new Combo();
        
        char name[260] = "Unnamed Combo";
        
        int i = 0;
        while (current != '\0')
        {
            name[i] = current;
            current = fgetc(f);
            i++;
        }
        name[i++] = '\0';

        combo->name = name;

        int32_t sample_count = 0;
        fread(&sample_count, sizeof(int32_t), 1, f);

        combo->samples = {};
        combo->samples.resize(sample_count);
        
        fread(combo->samples.data(), sizeof(uint32_t), sample_count, f);
        
        
        current = fgetc(f);
    }

    return combos;
}
