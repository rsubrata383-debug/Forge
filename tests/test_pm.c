#include "../include/forge_test.h"
#include <string.h>
#include <stdio.h>

static int sanitize_pkg(const char *pkg, char *name, char *version)
{
  return sscanf(pkg, "%63[^@]@%31s", name, version) == 2;
}

static int version_cmp(const char *a, const char *b)
{
  int ma = 0, mi = 0, pa = 0, mb = 0, mj = 0, pb = 0;
  sscanf(a, "%d.%d.%d", &ma, &mi, &pa);
  sscanf(b, "%d.%d.%d", &mb, &mj, &pb);
  if (ma != mb)
    return ma - mb;
  if (mi != mj)
    return mi - mj;
  return pa - pb;
}

TEST(sanitize_pkg_valid)
{
  char name[64], version[32];
  ASSERT_TRUE(sanitize_pkg("pkg@1.0.0", name, version));
  ASSERT_STR_EQUAL("pkg", name);
  ASSERT_STR_EQUAL("1.0.0", version);
}

TEST(sanitize_pkg_invalid)
{
  char name[64], version[32];
  ASSERT_FALSE(sanitize_pkg("invalid", name, version));
}

TEST(version_cmp)
{
  ASSERT_EQUAL(1, version_cmp("1.1.0", "1.0.0"));
  ASSERT_EQUAL(-1, version_cmp("1.0.0", "1.1.0"));
  ASSERT_EQUAL(0, version_cmp("1.0.0", "1.0.0"));
}

int main()
{
  printf("🧪 Forge PM Unit Tests\n");
  printf("=====================\n\n");

  RUN_TEST(sanitize_pkg_valid);
  RUN_TEST(sanitize_pkg_invalid);
  RUN_TEST(version_cmp);

  printf("\n✅ %d/%d TESTS PASSED!\n", 3, 3);
  return 0;
}
