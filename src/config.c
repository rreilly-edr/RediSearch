#include "config.h"
#include "err.h"
#include "rmutil/util.h"
#include "rmutil/strings.h"
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <assert.h>

#define RETURN_ERROR(s) return REDISMODULE_ERR;

static int readLongLong(RedisModuleString **argv, size_t argc, size_t *offset, long long *out) {
  assert(*offset <= argc);
  if (*offset == argc) {
    // printf("Missing argument!\n");
    RETURN_ERROR("Missing argument");
  }
  return RedisModule_StringToLongLong(argv[(*offset)++], out);
}

static int readLongLongLimit(RedisModuleString **argv, size_t argc, size_t *offset, long long *out,
                             long long minVal, long long maxVal) {
  if (readLongLong(argv, argc, offset, out) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  if (minVal != LLONG_MIN && *out < minVal) {
    // printf("TOO SMALL!\n");
    RETURN_ERROR("Value too small");
  }
  if (maxVal != LLONG_MAX && *out > maxVal) {
    // printf("TOO BIG!!\n");
    RETURN_ERROR("Value too big");
  }
  return REDISMODULE_OK;
}

#define CONFIG_SETTER(name) \
  static int name(RSConfig *config, RedisModuleString **argv, size_t argc, size_t *offset)

#define CONFIG_GETTER(name) static sds name(const RSConfig *config)

#define CONFIG_BOOLEAN_GETTER(name, var, invert) \
  CONFIG_GETTER(name) {                          \
    int cv = config->var;                        \
    if (invert) {                                \
      cv = !cv;                                  \
    }                                            \
    return sdsnew(cv ? "true" : "false");        \
  }

// EXTLOAD
CONFIG_SETTER(setExtLoad) {
  if (*offset >= argc) {
    return REDISMODULE_ERR;
  }
  config->extLoad = RedisModule_StringPtrLen(argv[(*offset)++], NULL);
  return REDISMODULE_OK;
}

CONFIG_GETTER(getExtLoad) {
  if (config->extLoad) {
    return sdsnew(config->extLoad);
  } else {
    return NULL;
  }
}

// SAFEMODE
CONFIG_SETTER(setSafemode) {
  config->concurrentMode = 0;
  return REDISMODULE_OK;
}

CONFIG_BOOLEAN_GETTER(getSafemode, concurrentMode, 1)

// NOGC
CONFIG_SETTER(setNoGc) {
  config->enableGC = 0;
  return REDISMODULE_OK;
}

CONFIG_BOOLEAN_GETTER(getNoGc, enableGC, 1)

// MINPREFIX
CONFIG_SETTER(setMinPrefix) {
  long long arg;
  if (readLongLongLimit(argv, argc, offset, &arg, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->minTermPrefix = arg;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getMinPrefix) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lld", config->minTermPrefix);
}

// MAXDOCTABLESIZE
CONFIG_SETTER(setMaxDocTableSize) {
  long long size;
  if (readLongLongLimit(argv, argc, offset, &size, 1, MAX_DOC_TABLE_SIZE) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->maxDocTableSize = size;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getMaxDocTableSize) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lu", config->maxDocTableSize);
}

// MAXEXPANSIONS
CONFIG_SETTER(setMaxExpansions) {
  long long num;
  if (readLongLongLimit(argv, argc, offset, &num, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->maxPrefixExpansions = num;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getMaxExpansions) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%llu", config->maxPrefixExpansions);
}

// TIMEOUT
CONFIG_SETTER(setTimeout) {
  long long val;
  if (readLongLongLimit(argv, argc, offset, &val, 0, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->queryTimeoutMS = val;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getTimeout) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lld", config->queryTimeoutMS);
}

// INDEX_THREADS
CONFIG_SETTER(setIndexThreads) {
  long long val;
  if (readLongLongLimit(argv, argc, offset, &val, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->indexPoolSize = val;
  config->poolSizeNoAuto = 1;
  return REDISMODULE_OK;
}
CONFIG_GETTER(getIndexthreads) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lu", config->indexPoolSize);
}

// INDEX_THREADS
CONFIG_SETTER(setSearchThreads) {
  long long val;
  if (readLongLongLimit(argv, argc, offset, &val, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->searchPoolSize = val;
  config->poolSizeNoAuto = 1;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getSearchThreads) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lu", config->searchPoolSize);
}

// FRISOINI
CONFIG_SETTER(setFrisoINI) {
  if (*offset == argc) {
    RETURN_ERROR("Missing argument");
  }
  const char *path = RedisModule_StringPtrLen(argv[(*offset)++], NULL);
  config->frisoIni = path;
  return REDISMODULE_OK;
}
CONFIG_GETTER(getFrisoINI) {
  return config->frisoIni ? sdsnew(config->frisoIni) : NULL;
}

// ON_TIMEOUT
CONFIG_SETTER(setOnTimeout) {
  if (*offset == argc) {
    RETURN_ERROR("Missing argument");
  }
  const char *policy = RedisModule_StringPtrLen(argv[(*offset)++], NULL);
  if (!strcasecmp(policy, "RETURN")) {
    config->timeoutPolicy = TimeoutPolicy_Return;
  } else if (!strcasecmp(policy, "FAIL")) {
    config->timeoutPolicy = TimeoutPolicy_Fail;
  } else {
    RETURN_ERROR("Invalid ON_TIMEOUT value");
    return REDISMODULE_ERR;
  }
  return REDISMODULE_OK;
}

CONFIG_GETTER(getOnTimeout) {
  return sdsnew(TimeoutPolicy_ToString(config->timeoutPolicy));
}

// GC_SCANSIZE
CONFIG_SETTER(setGcScanSize) {
  long long val;
  if (readLongLongLimit(argv, argc, offset, &val, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->gcScanSize = val;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getGcScanSize) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lu", config->gcScanSize);
}

// MIN_PHONETIC_TERM_LEN
CONFIG_SETTER(setMinPhoneticTermLen) {
  long long val;
  if (readLongLongLimit(argv, argc, offset, &val, 1, LLONG_MAX) != REDISMODULE_OK) {
    return REDISMODULE_ERR;
  }
  config->minPhoneticTermLen = val;
  return REDISMODULE_OK;
}

CONFIG_GETTER(getMinPhoneticTermLen) {
  sds ss = sdsempty();
  return sdscatprintf(ss, "%lu", config->minPhoneticTermLen);
}

RSConfig RSGlobalConfig = RS_DEFAULT_CONFIG;

static RSConfigVar *findConfigVar(const RSConfigVar *vars, const char *name) {
  for (; vars->name != NULL; vars++) {
    if (!strcmp(name, vars->name)) {
      return (RSConfigVar *)vars;
    }
  }
  return NULL;
}

int ReadConfig(RedisModuleString **argv, int argc, char **err) {
  *err = NULL;

  if (getenv("RS_MIN_THREADS")) {
    printf("Setting thread pool sizes to 1\n");
    RSGlobalConfig.searchPoolSize = 1;
    RSGlobalConfig.indexPoolSize = 1;
    RSGlobalConfig.poolSizeNoAuto = 1;
  }
  size_t offset = 0;
  while (offset < argc) {
    const char *name = RedisModule_StringPtrLen(argv[offset], NULL);
    RSConfigVar *curVar = findConfigVar(RSGlobalConfigOptions.vars, name);
    if (curVar == NULL) {
      asprintf(err, "No such configuration option `%s`", name);
      return REDISMODULE_ERR;
    }
    if (curVar->setValue == NULL) {
      asprintf(err, "%s: Option is read-only", name);
      return REDISMODULE_ERR;
    }

    offset++;
    if (curVar->setValue(&RSGlobalConfig, argv, argc, &offset) != REDISMODULE_OK) {
      asprintf(err, "%s: Bad value", name);
      return REDISMODULE_ERR;
    }
    // Mark the option as having been modified
    curVar->flags |= RSCONFIGVAR_F_MODIFIED;
  }
  return REDISMODULE_OK;
}

RSConfigOptions RSGlobalConfigOptions = {
    .vars = {
        {.name = "EXTLOAD",
         .helpText = "Load extension scoring/expansion module",
         .setValue = setExtLoad,
         .getValue = getExtLoad,
         .flags = RSCONFIGVAR_F_IMMUTABLE},
        {.name = "SAFEMODE",
         .helpText = "Perform all operations in main thread",
         .setValue = setSafemode,
         .getValue = getSafemode,
         .flags = RSCONFIGVAR_F_FLAG | RSCONFIGVAR_F_IMMUTABLE},
        {.name = "NOGC",
         .helpText = "Disable garbage collection (for this process)",
         .setValue = setNoGc,
         .getValue = getNoGc,
         .flags = RSCONFIGVAR_F_FLAG},
        {.name = "MINPREFIX",
         .helpText = "Set the minimum prefix for expansions (`*`)",
         .setValue = setMinPrefix,
         .getValue = getMinPrefix},
        {.name = "MAXDOCTABLESIZE",
         .helpText = "Maximum runtime document table size (for this process)",
         .setValue = setMaxDocTableSize,
         .getValue = getMaxDocTableSize,
         .flags = RSCONFIGVAR_F_IMMUTABLE},
        {.name = "MAXEXPANSIONS",
         .helpText = "Maximum prefix expansions to be used in a query",
         .setValue = setMaxExpansions,
         .getValue = getMaxExpansions},
        {.name = "TIMEOUT",
         .helpText = "Query (search) timeout",
         .setValue = setTimeout,
         .getValue = getTimeout},
        {.name = "INDEX_THREADS",
         .helpText = "Create at most this number of background indexing threads (will not "
                     "necessarily parallelize indexing)",
         .setValue = setIndexThreads,
         .getValue = getIndexthreads,
         .flags = RSCONFIGVAR_F_IMMUTABLE},
        {
            .name = "SEARCH_THREADS",
            .helpText = "Create at must this number of search threads (not, will not "
                        "necessarily parallelize search)",
            .setValue = setSearchThreads,
            .getValue = getSearchThreads,
            .flags = RSCONFIGVAR_F_IMMUTABLE,
        },
        {.name = "FRISOINI",
         .helpText = "Path to Chinese dictionary configuration file (for Chinese tokenization)",
         .setValue = setFrisoINI,
         .getValue = getFrisoINI,
         .flags = RSCONFIGVAR_F_IMMUTABLE},
        {.name = "ON_TIMEOUT",
         .helpText = "Action to perform when search timeout is exceeded (choose RETURN or FAIL)",
         .setValue = setOnTimeout,
         .getValue = getOnTimeout},
        {.name = "GCSCANSIZE",
         .helpText = "Scan this many documents at a time during every GC iteration",
         .setValue = setGcScanSize,
         .getValue = getGcScanSize},
        {.name = "MIN_PHONETIC_TERM_LEN",
         .helpText = "Minumum length of term to be considered for phonetic matching",
         .setValue = setMinPhoneticTermLen,
         .getValue = getMinPhoneticTermLen},
        {.name = NULL}}};

sds RSConfig_GetInfoString(const RSConfig *config) {
  sds ss = sdsempty();

  ss = sdscatprintf(ss, "concurrency: %s, ", config->concurrentMode ? "ON" : "OFF(SAFEMODE)");
  ss = sdscatprintf(ss, "gc: %s, ", config->enableGC ? "ON" : "OFF");
  ss = sdscatprintf(ss, "prefix min length: %lld, ", config->minTermPrefix);
  ss = sdscatprintf(ss, "prefix max expansions: %lld, ", config->maxPrefixExpansions);
  ss = sdscatprintf(ss, "query timeout (ms): %lld, ", config->queryTimeoutMS);
  ss = sdscatprintf(ss, "timeout policy: %s, ", TimeoutPolicy_ToString(config->timeoutPolicy));
  ss = sdscatprintf(ss, "cursor read size: %lld, ", config->cursorReadSize);
  ss = sdscatprintf(ss, "cursor max idle (ms): %lld, ", config->cursorMaxIdle);
  ss = sdscatprintf(ss, "max doctable size: %lu, ", config->maxDocTableSize);
  ss = sdscatprintf(ss, "search pool size: %lu, ", config->searchPoolSize);
  ss = sdscatprintf(ss, "index pool size: %lu, ", config->indexPoolSize);

  if (config->extLoad) {
    ss = sdscatprintf(ss, "ext load: %s, ", config->extLoad);
  }

  if (config->frisoIni) {
    ss = sdscatprintf(ss, "friso ini: %s, ", config->frisoIni);
  }
  return ss;
}

static void dumpConfigOption(const RSConfig *config, const RSConfigVar *var, RedisModuleCtx *ctx,
                             int isHelp) {
  size_t numElems = 0;
  sds currValue = var->getValue(config);

  RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
  RedisModule_ReplyWithSimpleString(ctx, var->name);
  numElems++;
  if (isHelp) {
    RedisModule_ReplyWithSimpleString(ctx, "Description");
    RedisModule_ReplyWithSimpleString(ctx, var->helpText);
    RedisModule_ReplyWithSimpleString(ctx, "Value");
    if (currValue) {
      RedisModule_ReplyWithStringBuffer(ctx, currValue, sdslen(currValue));
    } else {
      RedisModule_ReplyWithNull(ctx);
    }
    numElems += 4;
  } else {
    if (currValue) {
      RedisModule_ReplyWithSimpleString(ctx, currValue);
    } else {
      RedisModule_ReplyWithNull(ctx);
    }
    numElems++;
  }
  sdsfree(currValue);
  RedisModule_ReplySetArrayLength(ctx, numElems);
}

void RSConfig_DumpProto(const RSConfig *config, const RSConfigOptions *options, const char *name,
                        RedisModuleCtx *ctx, int isHelp) {
  size_t numElems = 0;
  RedisModule_ReplyWithArray(ctx, REDISMODULE_POSTPONED_ARRAY_LEN);
  if (!strcmp("*", name)) {
    for (const RSConfigVar *cur = &options->vars[0]; cur->name; cur++) {
      dumpConfigOption(config, cur, ctx, isHelp);
      numElems++;
    }
  } else {
    const RSConfigVar *v = findConfigVar(options->vars, name);
    if (v) {
      numElems++;
      dumpConfigOption(config, v, ctx, isHelp);
    }
  }
  RedisModule_ReplySetArrayLength(ctx, numElems);
}

int RSConfig_SetOption(RSConfig *config, RSConfigOptions *options, const char *name,
                       RedisModuleString **argv, int argc, size_t *offset, char **err) {
  RSConfigVar *var = findConfigVar(options->vars, name);
  if (!var) {
    SET_ERR(err, "No such option");
    return REDISMODULE_ERR;
  }
  if (var->flags & RSCONFIGVAR_F_IMMUTABLE) {
    SET_ERR(err, "Option not settable at runtime");
    return REDISMODULE_ERR;
  }
  return var->setValue(config, argv, argc, offset);
}