// This is an open source non-commercial project. Dear PVS-Studio, please check
// it. PVS-Studio Static Code Analyzer for C, C++ and C#: http://www.viva64.com

#include <stdbool.h>

#include "nvim/os/stdpaths_defs.h"
#include <string.h>
#include "nvim/os/os.h"
#include "nvim/path.h"
#include "nvim/memory.h"
#include "nvim/ascii.h"

/// Names of the environment variables, mapped to XDGVarType values
static const char *xdg_env_vars[] = {
  [kXDGConfigHome] = "XDG_CONFIG_HOME",
  [kXDGDataHome] = "XDG_DATA_HOME",
  [kXDGCacheHome] = "XDG_CACHE_HOME",
  [kXDGRuntimeDir] = "XDG_RUNTIME_DIR",
  [kXDGConfigDirs] = "XDG_CONFIG_DIRS",
  [kXDGDataDirs] = "XDG_DATA_DIRS",
};

#ifdef WIN32
static const char *const xdg_defaults_env_vars[] = {
  [kXDGConfigHome] = "LOCALAPPDATA",
  [kXDGDataHome] = "LOCALAPPDATA",
  [kXDGCacheHome] = "TEMP",
  [kXDGRuntimeDir] = NULL,
  [kXDGConfigDirs] = NULL,
  [kXDGDataDirs] = NULL,
};
#endif

/// Defaults for XDGVarType values
///
/// Used in case environment variables contain nothing. Need to be expanded.
static const char *const xdg_defaults[] = {
#ifdef WIN32
  [kXDGConfigHome] = "~\\AppData\\Local",
  [kXDGDataHome] = "~\\AppData\\Local",
  [kXDGCacheHome] = "~\\AppData\\Local\\Temp",
  [kXDGRuntimeDir] = NULL,
  [kXDGConfigDirs] = NULL,
  [kXDGDataDirs] = NULL,
#else
  [kXDGConfigHome] = "~/.config",
  [kXDGDataHome] = "~/.local/share",
  [kXDGCacheHome] = "~/.cache",
  [kXDGRuntimeDir] = NULL,
  [kXDGConfigDirs] = "/etc/xdg/",
  [kXDGDataDirs] = "/usr/local/share/:/usr/share/",
#endif
};

/// Return XDG variable value
///
/// @param[in]  idx  XDG variable to use.
///
/// @return [allocated] variable value.
char *stdpaths_get_xdg_var(const XDGVarType idx)
  FUNC_ATTR_WARN_UNUSED_RESULT
{
  const char *const env = xdg_env_vars[idx];
  const char *const fallback = xdg_defaults[idx];

  const char *env_val = os_getenv(env);

#ifdef WIN32
  if (env_val == NULL && xdg_defaults_env_vars[idx] != NULL) {
    env_val = os_getenv(xdg_defaults_env_vars[idx]);
  }
#else
  if (env_val == NULL && os_env_exists(env)) {
    env_val = "";
  }
#endif

  char *ret = NULL;
  if (env_val != NULL) {
    ret = xstrdup(env_val);
  } else if (fallback) {
    ret = (char *)expand_env_save((char_u *)fallback);
  }
  if (idx == kXDGDataDirs || idx == kXDGConfigDirs) {
      ret = remove_duplicate_directories(ret);
  }

  return ret;
}

/// Remove duplicate directories in the given string.
/// e.g, "/usr/local/share:/usr/share:/usr/share"
/// to "/usr/local/share:/usr/share"
/// @param[in]  List of directories possibly with duplicates
/// @param[out]  List of directories without duplicates
char *remove_duplicate_directories(const char *val)
  FUNC_ATTR_WARN_UNUSED_RESULT
{
  char *ret = (char *)xmalloc((strlen(val) + 1) * sizeof(char));
  ret[0] = '\0';
  const void *iter = NULL;
  do {
    size_t dir_len;
    const char *dir;
    iter = vim_env_iter(':', val, iter, &dir, &dir_len);
    if (dir != NULL && dir_len > 0) {
      const void *ret_iter = NULL;
      int seen = 0;
      do {
        size_t ret_dir_len;
        const char *ret_dir;
        ret_iter = vim_env_iter(':', ret, ret_iter, &ret_dir, &ret_dir_len);
        if (ret_dir != NULL && ret_dir_len == dir_len) {
          if (!strncmp(ret_dir, dir, dir_len)) {
            seen = 1;
            break;
          }
        }
      } while (ret_iter != NULL);
      if (!seen) {
        if (strlen(ret) > 0) {
          strcat(ret, ":");
        }
        strncat(ret, dir, dir_len);
      }
    }
  } while (iter != NULL);
  return ret;
}

/// Return Nvim-specific XDG directory subpath.
///
/// Windows: Uses "…/nvim-data" for kXDGDataHome to avoid storing
/// configuration and data files in the same path. #4403
///
/// @param[in]  idx  XDG directory to use.
///
/// @return [allocated] "{xdg_directory}/nvim"
char *get_xdg_home(const XDGVarType idx)
  FUNC_ATTR_WARN_UNUSED_RESULT
{
  char *dir = stdpaths_get_xdg_var(idx);
  if (dir) {
#if defined(WIN32)
    dir = concat_fnames_realloc(dir,
                                (idx == kXDGDataHome ? "nvim-data" : "nvim"),
                                true);
#else
    dir = concat_fnames_realloc(dir, "nvim", true);
#endif
  }
  return dir;
}

/// Return subpath of $XDG_CONFIG_HOME
///
/// @param[in]  fname  New component of the path.
///
/// @return [allocated] `$XDG_CONFIG_HOME/nvim/{fname}`
char *stdpaths_user_conf_subpath(const char *fname)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_ALL FUNC_ATTR_NONNULL_RET
{
  return concat_fnames_realloc(get_xdg_home(kXDGConfigHome), fname, true);
}

/// Return subpath of $XDG_DATA_HOME
///
/// @param[in]  fname  New component of the path.
/// @param[in]  trailing_pathseps  Amount of trailing path separators to add.
/// @param[in]  escape_commas  If true, all commas will be escaped.
///
/// @return [allocated] `$XDG_DATA_HOME/nvim/{fname}`.
char *stdpaths_user_data_subpath(const char *fname,
                                 const size_t trailing_pathseps,
                                 const bool escape_commas)
  FUNC_ATTR_WARN_UNUSED_RESULT FUNC_ATTR_NONNULL_ALL FUNC_ATTR_NONNULL_RET
{
  char *ret = concat_fnames_realloc(get_xdg_home(kXDGDataHome), fname, true);
  const size_t len = strlen(ret);
  const size_t numcommas = (escape_commas ? memcnt(ret, ',', len) : 0);
  if (numcommas || trailing_pathseps) {
    ret = xrealloc(ret, len + trailing_pathseps + numcommas + 1);
    for (size_t i = 0 ; i < len + numcommas ; i++) {
      if (ret[i] == ',') {
        memmove(ret + i + 1, ret + i, len - i + numcommas);
        ret[i] = '\\';
        i++;
      }
    }
    if (trailing_pathseps) {
      memset(ret + len + numcommas, PATHSEP, trailing_pathseps);
    }
    ret[len + trailing_pathseps + numcommas] = NUL;
  }
  return ret;
}
