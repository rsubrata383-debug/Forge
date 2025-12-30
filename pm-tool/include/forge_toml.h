typedef struct
{
  char *name;
  char *version;
} Dep;

typedef struct
{
  char *name;
  char *version;
  Dep *deps;
  size_t deps_count;
} ForgeToml;

ForgeToml *forge_toml_parse(const char *path);
void forge_toml_free(ForgeToml *toml);
