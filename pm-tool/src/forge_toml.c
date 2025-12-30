#include "../include/forge_toml.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
ForgeToml *forge_toml_parse(const char *path)
{
  FILE *f = fopen(path, "r");
  // Parse [project] name = "user-app"
  // Parse [dependencies] ui = "1.0.0"
  fclose(f);
  return toml;
}
