#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <Eet.h>
#include <Eina.h>
#include <Ecore_File.h>
#include <Efreet.h>
#include <Elementary.h>

#include "edi_config.h"

#include "edi_private.h"

# define EDI_CONFIG_LIMIT(v, min, max) \
   if (v > max) v = max; else if (v < min) v = min;

# define EDI_CONFIG_DD_NEW(str, typ) \
   _edi_config_descriptor_new(str, sizeof(typ))

# define EDI_CONFIG_DD_FREE(eed) \
   if (eed) { eet_data_descriptor_free(eed); (eed) = NULL; }

# define EDI_CONFIG_VAL(edd, type, member, dtype) \
   EET_DATA_DESCRIPTOR_ADD_BASIC(edd, type, #member, member, dtype)

# define EDI_CONFIG_SUB(edd, type, member, eddtype) \
   EET_DATA_DESCRIPTOR_ADD_SUB(edd, type, #member, member, eddtype)

# define EDI_CONFIG_LIST(edd, type, member, eddtype) \
   EET_DATA_DESCRIPTOR_ADD_LIST(edd, type, #member, member, eddtype)

# define EDI_CONFIG_HASH(edd, type, member, eddtype) \
   EET_DATA_DESCRIPTOR_ADD_HASH(edd, type, #member, member, eddtype)

#  define EDI_CONFIG_FILE_EPOCH 0x0002
#  define EDI_CONFIG_FILE_GENERATION 0x000b
#  define EDI_CONFIG_FILE_VERSION \
   ((EDI_CONFIG_FILE_EPOCH << 16) | EDI_CONFIG_FILE_GENERATION)

typedef Eet_Data_Descriptor Edi_Config_DD;

/* local variables */
static Edi_Config_DD *_edi_cfg_edd = NULL;
static Edi_Config_DD *_edi_cfg_proj_edd = NULL;
static Edi_Config_DD *_edi_cfg_mime_edd = NULL;

/* external variables */
Edi_Config *_edi_cfg = NULL;
int EDI_EVENT_CONFIG_CHANGED;

const char *
_edi_config_dir_get(void)
{
   static char dir[PATH_MAX];

   if (!dir[0])
     snprintf(dir, sizeof(dir), "%s/edi", efreet_config_home_get());

   return dir;
}

/* local functions */
static Edi_Config_DD *
_edi_config_descriptor_new(const char *name, int size)
{
   Eet_Data_Descriptor_Class eddc;

   if (!eet_eina_stream_data_descriptor_class_set(&eddc, sizeof(eddc), 
                                                  name, size))
     return NULL;

   return (Edi_Config_DD *)eet_data_descriptor_stream_new(&eddc);
}

static void
_edi_config_cb_free(void)
{
   Edi_Config_Project *proj;
   Edi_Config_Mime_Association *mime;

   EINA_LIST_FREE(_edi_cfg->projects, proj)
     {
        if (proj->name) eina_stringshare_del(proj->name);
        if (proj->path) eina_stringshare_del(proj->path);
        free(proj);
     }

   EINA_LIST_FREE(_edi_cfg->mime_assocs, mime)
     {
        if (mime->id) eina_stringshare_del(mime->id);
        if (mime->mime) eina_stringshare_del(mime->mime);
        free(mime);
     }

   free(_edi_cfg);
   _edi_cfg = NULL;
}

static void *
_edi_config_domain_load(const char *domain, Edi_Config_DD *edd)
{
   Eet_File *ef;
   char buff[PATH_MAX];

   if (!domain) return NULL;
   snprintf(buff, sizeof(buff),
            "%s/%s.cfg", _edi_config_dir_get(), domain);
   ef = eet_open(buff, EET_FILE_MODE_READ);
   if (ef)
     {
        void *data;

        data = eet_data_read(ef, edd, "config");
        eet_close(ef);
        if (data) return data;
     }
   return NULL;
}

static Eina_Bool 
_edi_config_domain_save(const char *domain, Edi_Config_DD *edd, const void *data)
{
   Eet_File *ef;
   char buff[PATH_MAX];
   const char *configdir;

   if (!domain) return 0;
   configdir = _edi_config_dir_get();
   snprintf(buff, sizeof(buff), "%s/", configdir);
   if (!ecore_file_exists(buff)) ecore_file_mkpath(buff);
   snprintf(buff, sizeof(buff), "%s/%s.tmp", configdir, domain);
   ef = eet_open(buff, EET_FILE_MODE_WRITE);
   if (ef)
     {
        char buff2[PATH_MAX];

        snprintf(buff2, sizeof(buff2), "%s/%s.cfg", configdir, domain);
        if (!eet_data_write(ef, edd, "config", data, 1))
          {
             eet_close(ef);
             return EINA_FALSE;
          }
        if (eet_close(ef) > 0) return EINA_FALSE;
        if (!ecore_file_mv(buff, buff2)) return EINA_FALSE;
        return EINA_TRUE;
     }

   return EINA_FALSE;
}

/* external functions */
Eina_Bool 
_edi_config_init(void)
{
   elm_need_efreet();
   if (!efreet_init()) return EINA_FALSE;
   EDI_EVENT_CONFIG_CHANGED = ecore_event_type_new();

   _edi_cfg_proj_edd = EDI_CONFIG_DD_NEW("Config_Project", Edi_Config_Project);
   #undef T
   #undef D
   #define T Edi_Config_Project
   #define D _edi_cfg_proj_edd
   EDI_CONFIG_VAL(D, T, name, EET_T_STRING);
   EDI_CONFIG_VAL(D, T, path, EET_T_STRING);

   _edi_cfg_mime_edd = EDI_CONFIG_DD_NEW("Config_Mime", Edi_Config_Mime_Association);
   #undef T
   #undef D
   #define T Edi_Config_Mime_Association
   #define D _edi_cfg_mime_edd
   EDI_CONFIG_VAL(D, T, id, EET_T_STRING);
   EDI_CONFIG_VAL(D, T, mime, EET_T_STRING);

   _edi_cfg_edd = EDI_CONFIG_DD_NEW("Config", Edi_Config);
   #undef T
   #undef D
   #define T Edi_Config
   #define D _edi_cfg_edd
   EDI_CONFIG_VAL(D, T, version, EET_T_INT);
   EDI_CONFIG_VAL(D, T, font.size, EET_T_INT);
   EDI_CONFIG_VAL(D, T, gui.translucent, EET_T_UCHAR);
   EDI_CONFIG_VAL(D, T, gui.width, EET_T_INT);
   EDI_CONFIG_VAL(D, T, gui.height, EET_T_INT);
   EDI_CONFIG_VAL(D, T, gui.leftsize, EET_T_DOUBLE);
   EDI_CONFIG_VAL(D, T, gui.leftopen, EET_T_UCHAR);
   EDI_CONFIG_VAL(D, T, gui.bottomsize, EET_T_DOUBLE);
   EDI_CONFIG_VAL(D, T, gui.bottomopen, EET_T_UCHAR);
   EDI_CONFIG_VAL(D, T, gui.bottomtab, EET_T_INT);
   EDI_CONFIG_VAL(D, T, gui.show_whitespace, EET_T_UCHAR);
   EDI_CONFIG_VAL(D, T, gui.width_marker, EET_T_UINT);
   EDI_CONFIG_VAL(D, T, gui.tabstop, EET_T_UINT);
   EDI_CONFIG_VAL(D, T, autosave, EET_T_UCHAR);
   EDI_CONFIG_LIST(D, T, projects, _edi_cfg_proj_edd);
   EDI_CONFIG_LIST(D, T, mime_assocs, _edi_cfg_mime_edd);

   _edi_config_load();

   return EINA_TRUE;
}

Eina_Bool 
_edi_config_shutdown(void)
{
   _edi_config_cb_free();

   EDI_CONFIG_DD_FREE(_edi_cfg_proj_edd);
   EDI_CONFIG_DD_FREE(_edi_cfg_mime_edd);
   EDI_CONFIG_DD_FREE(_edi_cfg_edd);

   efreet_shutdown();

   return EINA_TRUE;
}

void 
_edi_config_load(void)
{
   Eina_Bool save = EINA_FALSE;

   _edi_cfg = _edi_config_domain_load(PACKAGE_NAME, _edi_cfg_edd);
   if (_edi_cfg)
     {
        Eina_Bool reload = EINA_FALSE;

        if ((_edi_cfg->version >> 16) < EDI_CONFIG_FILE_EPOCH)
          {
             /* config too old */
             reload = EINA_TRUE;
          }
        else if (_edi_cfg->version > EDI_CONFIG_FILE_VERSION)
          {
             /* config too new, WTF ? */
             reload = EINA_TRUE;
          }

        /* if too old or too new, clear it so we can create new */
        if (reload) _edi_config_cb_free();
     }

   if (!_edi_cfg) 
     {
        _edi_cfg = calloc(1, sizeof(Edi_Config));
        save = EINA_TRUE;
     }

   /* define some convenient macros */
#define IFCFG(v) if ((_edi_cfg->version & 0xffff) < (v)) {
#define IFCFGELSE } else {
#define IFCFGEND }

   /* setup defaults */
   IFCFG(0x000a);

   _edi_cfg->font.size = 12;
   _edi_cfg->gui.translucent = EINA_TRUE;
   _edi_cfg->gui.width = 640;
   _edi_cfg->gui.height = 480;
   _edi_cfg->gui.leftsize = 0.25;
   _edi_cfg->gui.leftopen = EINA_TRUE;
   _edi_cfg->gui.bottomsize = 0.2;
   _edi_cfg->gui.bottomopen = EINA_FALSE;
   _edi_cfg->gui.bottomtab = 0;
   _edi_cfg->autosave = EINA_TRUE;
   _edi_cfg->projects = NULL;
   _edi_cfg->mime_assocs = NULL;
   IFCFGEND;

   IFCFG(0x000b);
   _edi_cfg->gui.width_marker = 80;
   _edi_cfg->gui.tabstop = 8;
   IFCFGEND;

   /* limit config values so they are sane */
   EDI_CONFIG_LIMIT(_edi_cfg->font.size, 8, 96);
   EDI_CONFIG_LIMIT(_edi_cfg->gui.width, 150, 10000);
   EDI_CONFIG_LIMIT(_edi_cfg->gui.height, 100, 8000);
   EDI_CONFIG_LIMIT(_edi_cfg->gui.leftsize, 0.0, 1.0);
   EDI_CONFIG_LIMIT(_edi_cfg->gui.bottomsize, 0.0, 1.0);
   EDI_CONFIG_LIMIT(_edi_cfg->gui.tabstop, 1, 32);

   _edi_cfg->version = EDI_CONFIG_FILE_VERSION;

   if (save) _edi_config_save();
}

void 
_edi_config_save(void)
{
   if (_edi_config_domain_save(PACKAGE_NAME, _edi_cfg_edd, _edi_cfg))
     ecore_event_add(EDI_EVENT_CONFIG_CHANGED, NULL, NULL, NULL);
}

void
_edi_config_project_add(const char *path)
{
   Edi_Config_Project *project;
   Eina_List *list, *next;

   EINA_LIST_FOREACH_SAFE(_edi_cfg->projects, list, next, project)
     {
        if (!strncmp(project->path, path, strlen(project->path)))
          _edi_cfg->projects = eina_list_remove_list(_edi_cfg->projects, list);
     }

   project = malloc(sizeof(*project));
   project->path = eina_stringshare_add(path);
   project->name = eina_stringshare_add(basename((char*) path));
   _edi_cfg->projects = eina_list_prepend(_edi_cfg->projects, project);
   _edi_config_save();
}

void
_edi_config_project_remove(const char *path)
{
   Edi_Config_Project *project;
   Eina_List *list, *next;

   EINA_LIST_FOREACH_SAFE(_edi_cfg->projects, list, next, project)
     {
        if (!strncmp(project->path, path, strlen(project->path)))
	  {
	     break;
	  }
     }

   _edi_cfg->projects = eina_list_remove(_edi_cfg->projects, project);
   _edi_config_save();
}

void
_edi_config_mime_add(const char *mime, const char *id)
{
   Edi_Config_Mime_Association *mime_assoc;

   mime_assoc = malloc(sizeof(*mime_assoc));
   mime_assoc->id = eina_stringshare_add(id);
   mime_assoc->mime = eina_stringshare_add(mime);
   _edi_cfg->mime_assocs = eina_list_prepend(_edi_cfg->mime_assocs, mime_assoc);
   _edi_config_save();
}

const char *
_edi_config_mime_search(const char *mime)
{
   Edi_Config_Mime_Association *mime_assoc;
   Eina_List *list, *next;

   EINA_LIST_FOREACH_SAFE(_edi_cfg->mime_assocs, list, next, mime_assoc)
     {
        if (!strncmp(mime_assoc->mime, mime, strlen(mime_assoc->mime)))
          {
             return mime_assoc->id;
          }
     }
   return NULL;
}
