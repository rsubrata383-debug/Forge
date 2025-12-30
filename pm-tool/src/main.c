#include "../include/forge_toml.h"
#include "../include/forge_pm.h"

int main(int argc, char **argv)
{
  if (strcmp(argv[1], "install") == 0)
  {
    ForgeToml *toml = forge_toml_parse("forge.toml");
    for (size_t i = 0; i < toml->deps_count; i++)
    {
      printf("Installing %s@%s\n", toml->deps[i].name, toml->deps[i].version);
      // forge_pm_install(toml->deps[i]);
    }
    forge_toml_free(toml);
  }
}
