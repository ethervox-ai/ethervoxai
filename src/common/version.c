#include "ethervox/version.h"
#include "ethervox/config.h"

const char *ethervox_core_version(void) {
  return ETHERVOX_VERSION_STRING;
}

const char *ethervox_core_git_commit(void) {
  return ETHERVOX_GIT_COMMIT;
}
