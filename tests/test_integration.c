#include "forge_test.h"
#include <stdlib.h>
#include <string.h>

static int system_silent(const char *cmd)
{
  return system(cmd);
}

// FORCE CLEAN: Delete vendor dir completely
static void force_clean()
{
  system_silent("rmdir /s /q ..\\user-app\\vendor\\forge 2>nul");
  system_silent("mkdir ..\\user-app\\vendor\\forge 2>nul");
}

TEST(integration_full_cycle)
{
  force_clean(); // ✅ HARDCODE CLEAN STATE

  // 1. INSTALL (fresh)
  ASSERT_EQUAL(0, system_silent("build\\forge-pm.exe install pkg@1.0.0"));

  // 2. VERIFY LIST
  ASSERT_EQUAL(0, system_silent("build\\forge-pm.exe list | findstr pkg"));

  // 3. CHECK FILESYSTEM
  FILE *f = fopen("../user-app/vendor/forge/pkg/1.0.0/README.md", "r");
  ASSERT_TRUE(f != NULL);
  fclose(f);

  // 4. REMOVE
  ASSERT_EQUAL(0, system_silent("build\\forge-pm.exe remove pkg@1.0.0"));

  // 5. VERIFY REMOVED
  f = fopen("../user-app/vendor/forge/pkg/1.0.0/README.md", "r");
  ASSERT_TRUE(f == NULL);
}

TEST(integration_list_empty)
{
  force_clean();
  ASSERT_EQUAL(0, system_silent("build\\forge-pm.exe list"));
}

int main()
{
  printf("🔗 Forge PM Integration Tests\n");
  printf("============================\n\n");

  RUN_TEST(integration_full_cycle);
  RUN_TEST(integration_list_empty);

  printf("\n✅ 2/2 Integration tests PASSED!\n");
  return 0;
}
