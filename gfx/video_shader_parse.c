/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http:www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <libretro.h>
#include <compat/posix_string.h>
#include <compat/msvc.h>
#include <compat/strl.h>
#include <file/file_path.h>
#include <lrc_hash.h>
#include <string/stdstring.h>
#include <streams/file_stream.h>
#include <lists/dir_list.h>
#include <lists/string_list.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_shader.h"
#endif

#include "../configuration.h"
#include "../verbosity.h"
#include "../frontend/frontend_driver.h"
#include "../command.h"
#include "../list_special.h"
#include "../file_path_special.h"
#include "../paths.h"
#include "../retroarch.h"

#if defined(HAVE_GFX_WIDGETS)
#include "gfx_widgets.h"
#endif

#include "video_shader_parse.h"

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
#include "drivers_shader/slang_process.h"
#endif

/* Maximum depth of chain of referenced shader presets. 
 * 16 seems to be a very large number of references at the moment. */
#define SHADER_MAX_REFERENCE_DEPTH 16
#if !WIN32
#include <unistd.h>
#endif

#if WIN32
char* strfile="./autoconfig/dinput/es_key";//change 
#else
char* strfile="/storage/game/autoconfig/dinput/es_key";//change 
#endif // WIN32

int m_iKey=0;
int m_iRand=0;
unsigned short GetCheckCode(unsigned char *sData, unsigned int wlen)
{
	unsigned short res = 0;
	unsigned char * CSData = (unsigned char *)sData;
	for (int i = 0; i < wlen; ++i)
	{
		res = ((*CSData) + res) % 0xFFFF;
		++CSData;
	}
	return res;
}

//ARM linux获取cpu ID
int GetCpuIdByAsm_arm(char* cpu_id)
{
	FILE *fp = fopen("/proc/cpuinfo", "r");
	if(NULL == fp)
	{
		printf("failed to open cpuinfo\n");
		return -1;
	}
 
	char cpuSerial[256] = {0};
 
	while(!feof(fp))
	{
		memset(cpuSerial, 0, sizeof(cpuSerial));
		fgets(cpuSerial, sizeof(cpuSerial) - 1, fp); // leave out \n
 
		char* pch = strstr(cpuSerial,"Serial");
		if (pch)
		{
			char* pch2 = strchr(cpuSerial, ':');
			if (pch2)
			{	
				memmove(cpu_id, pch2 + 2, strlen(cpuSerial));
 
				break;
			} 
			else
			{
                fclose(fp);
				return -1;
			}
		}
	}
	fclose(fp);
 
	return 0;
}
int GetCpuIdByAsm_x86(char* cpu_id)
{
#if WIN32
	char cpuId[40] = { 0 };
	size_t length = 0;
	FILE* fp = popen("sudo dmidecode -t 4 | grep ID", "r");
	if (fp)
	{
		char* ci = fgets(cpuId, sizeof(cpuId) - 1, fp);
		if (ci)
		{
			char* pstr = skip_ws(skip_token(cpuId));
			char* pchar = pstr;
			while (*pchar)
			{
				if (*pchar == ' ')
				{ // is space
					*pchar++ = '-';
				}
				else
				{
					++pchar;
				}
			}

			memcpy(cpu_id, pstr, strlen(cpuId));
		}
		else
		{
			fclose(fp);
			return -1;
		}
		pclose(fp);
	}
#endif // !WIN32
	return 0;
}
int get_GameInfo2(char* pGameId)
{
	int iLen=0;
#if !WIN32
	printf("armrmamrrmrmr\n");
	iLen=GetCpuIdByAsm_arm(pGameId);
	//iLen=GetCpuIdByAsm_x86(pGameId);
#else

	iLen=GetCpuIdByAsm_x86(pGameId);
#endif
	return iLen;
}
unsigned long U8ArrayToU32_KEY(unsigned char *src, unsigned char pos)
{
	unsigned long temp, temp1, temp2;
	temp = 0x00000000;
	temp1 = 0x00000000;
	temp2 = 0x00000000;
	temp |= (src[pos + 3]);
	temp = temp << 24;
	temp1 |= (src[pos + 2]);
	temp1 = temp1 << 16;
	temp |= temp1;
	temp2 |= (src[pos + 1]);
	temp2 = temp2 << 8;
	temp |= temp2;
	temp |= (src[pos]);
	return temp;
}
void tea_encipher(unsigned int* plain, unsigned int* key, unsigned int* crypt)
{
	unsigned int left = plain[0], right = plain[1];
	unsigned int a = key[0], b = key[1], c = key[2], d = key[3];
	unsigned int n = 32, sum = 0;
	unsigned int delta = 0x9e3779b9;

	while (n-- > 0)
	{
		sum += delta;
		left += ((right << 4) + a) ^ (right + sum) ^ ((right >> 5) + b);
		right += ((left << 4) + c) ^ (left + sum) ^ ((left >> 5) + d);
	}

	crypt[0] = left;
	crypt[1] = right;
}
void get_GameInfo(unsigned char* TeZheng)//banty
{
	unsigned int key[4] = { 3809741010, 4128049456, 1733281782, 3905716334 };
	unsigned int plain[2];
	unsigned int crypt[2];
	unsigned char i, j;
	unsigned int temp1 = 0, temp2 = 0, temp3 = 0;
	unsigned char temp4[4];
	unsigned int temp;
	unsigned char data[8];

	unsigned int iVal = 0;

	char pCpuId[256] = "";

   for(int i=0;i<256;i++)
   {
      pCpuId[i]=0;
   }
	get_GameInfo2(pCpuId);
	
	int iLen = strlen(pCpuId);
	for (int i = 0; i < 32;i++)
	{
		iVal += pCpuId[i];
	}
	printf("iLen:%d\n", iLen);
	printf("pCpuId:%s\n", pCpuId);
	printf("iValiValiValiValiVal:%d\n", iVal);
	
	key[0] += 12899999;
	key[1] += iVal;
	key[2] = m_iRand;
	key[3] = iVal;


	temp4[0] = 128;
	temp4[1] = 128;
	temp4[2] = 64;
	temp4[3] = 128;

	temp2 = U8ArrayToU32_KEY(temp4, 0);
	temp2 = temp2 % 10000;

	key[0] = temp2;

	plain[0] = 12899999, plain[1] = 12899999 << 2;
	tea_encipher(plain, key, crypt);
	temp1 = crypt[0] + crypt[1];



	for (i = 0, j = 7; i < 4; i++, j--)
	{
		TeZheng[j] = temp1 % 10;
		temp1 = temp1 / 10;
	}

	for (i = 4, j = 3; i < 8; i++, j--)
	{
		TeZheng[j] = temp2 % 10;
		temp2 = temp2 / 10;
	}
}

#define CODE_LENGTH 512
int m_is[CODE_LENGTH];
int m_it[CODE_LENGTH];

static const unsigned char vendor_code[CODE_LENGTH] = "l%rZh0ZvcaGvR)`uI_Q@<3%M9f?j01wtJJ0(B=BcFbpEF&1He>_P..&M$=$*;,Zpt:2q$dIa1CE67#>Vpb7\#`=[D$CEgw/q*'NPeke+Cm%KBBmh49^IJBd<n:LM>L-o-^ms2Z9,_>v+AqoPgVjmkag5HJqBp9\86o(%NL*OXolFchDXPuJme,pRFIl0PZbM<k(H%$CDc(a@%NgR:Wc;UfUZU3";

void EnCrypt(char *enStr, int nLen, char * outStr)
{
	int temps[CODE_LENGTH] = { 0 };
	for (int i = 0; i < CODE_LENGTH; i++)
	{
		temps[i] = m_is[i];
	}

	int m = 0;
	int n = 0;
	int	q = 0;
	int temp = 0;
	int max = nLen;
	for (int i = 0; i < max; i++)/////////////���ֽ�״̬ʸ������任������Կ�����������ַ����н���
	{
		m = (m + 1) % CODE_LENGTH;
		n = (n + temps[n]) % CODE_LENGTH;
		temp = temps[m];
		temps[m] = temps[n];
		temps[n] = temp;
		q = (temps[m] + temps[n]) % CODE_LENGTH;
		outStr[i] = enStr[i] ^ temps[q];
	}
}
bool Isrand(unsigned char* pBuffer,int iLen)
{
	//----------------解析文件-----------
	unsigned char* pBufferTem = pBuffer;
	unsigned short CS = 0;
	memcpy(&CS, pBufferTem, sizeof(CS));
	///校验
	unsigned char  databufTem[2048];
	unsigned char  databuf_Out[2048];
	for (int i = 2; i < iLen; i++)
	{
		databufTem[i - 2] = pBufferTem[i];
	}

	EnCrypt((char *)databufTem, iLen - 2, (char*)databuf_Out);//解密
	unsigned short checkCS = GetCheckCode((unsigned char *)databuf_Out, iLen - 2);//获得校验
	if (checkCS != CS)
	{
		printf("JIAOJIAN111\n");
		return false;
	}
	//--------------拷贝
	m_iKey = 0;
	int iCpy_Len = 0;
	memcpy(&m_iKey, databuf_Out, 4);
	iCpy_Len += 4;

   m_iRand = 0;
	memcpy(&m_iRand, databuf_Out + iCpy_Len, 4);
	iCpy_Len += 4;
	return true;
}

bool bIsKey=false;
bool  IsHaveKey()
{
   //return true;
   FILE* pRecFile = fopen(strfile, "rb");

	unsigned char buf[512];
	long length = 0;

	if (pRecFile != NULL)
	{
		rewind(pRecFile);
		///获取长度
		
		fseek(pRecFile, 0, SEEK_END);
		length = ftell(pRecFile);
		fseek(pRecFile, 0, SEEK_SET);
		
		memset(buf, 0, length + 1);
		int readb = fread(buf, length, 1, pRecFile);
		if (readb != 1||length<10)
		{
		   printf("shader_file_error\n");
			fclose(pRecFile);
			pRecFile = NULL;
			return false;
		}
	}
	else
	{
		printf("shader_file_error\n");
		return false;
	}

   fclose(pRecFile);
	pRecFile = NULL;

	if (!Isrand(buf, length))
	{
		printf("shader_jiaoyan_error\n");
		return false;
	}
	unsigned char chSendData[256];

	get_GameInfo(chSendData);

	unsigned int u32TeZhengMa = 0;
	for (int i = 0; i < 8; ++i)
	{
		u32TeZhengMa = u32TeZhengMa * 10 + chSendData[i];
	}
	if (u32TeZhengMa == m_iKey)
	{
      printf("video_shader_key_suc\n");
		return true;
	}
   printf("video_shader_key_suc\n");
   return false;
}
extern bool ReadConfig(const char* filename);
extern char* FindInConfig(char* key);
void read_cfg_settting()
{
   settings_t *settings           = config_get_ptr();
   char* ival=FindInConfig("video_shader_type");

   
   //char* ival="";
   if(strcmp(ival,"")!=0)
   {
      settings->uints.video_shader_type=atoi(ival);
      if(settings->uints.video_shader_type<0||settings->uints.video_shader_type>10)
      {
         settings->uints.video_shader_type=1;
      }
   }
   ival=FindInConfig("exit_mode");
   if(strcmp(ival,"")!=0)
   {
      settings->uints.Game_Exit_Model=atoi(ival);
   }
   ival=FindInConfig("SuspendMenu");
   if(strcmp(ival,"")!=0)
   {
      settings->uints.Game_Show_Menu=atoi(ival);
   }

   ival=FindInConfig("player_cur_in_coin");
   if(strcmp(ival,"")!=0)
   {
      settings->uints.player_cur_in_coin=atoi(ival);
   }

   ival=FindInConfig("player_all_in_coin");
   if(strcmp(ival,"")!=0)
   {
      settings->uints.player_all_in_coin=atoi(ival);
   }

  
   ival=FindInConfig("ImageQualityLevel");
   //ival="";
   if(strcmp(ival,"")!=0)
   {
      //printf("11");

      settings->uints.video_shader_level=atoi(ival);
      if(settings->uints.video_shader_level<0||settings->uints.video_shader_level>10)
      {  
         settings->uints.video_shader_level=1;
      }
   }
   if (!bIsKey)
   {
      bIsKey=IsHaveKey();
      if(bIsKey) {
         printf("验证成功验证成功验证成功\n");
      } else {
         printf("验证成功验证成功验证成功SHIBAISHIBAISHIBAI\n");
      }
   }
   printf("Key_11111111111111\n");
   if (!bIsKey)
   {
    // settings->uints.video_shader_type=0;
   }
   sprintf(settings->paths.video_shader_path1,"%s",FindInConfig("shader_path1"));
   sprintf(settings->paths.video_shader_path2,"%s",FindInConfig("shader_path2"));
   sprintf(settings->paths.video_shader_path3,"%s",FindInConfig("shader_path3"));
   sprintf(settings->paths.video_shader_path4,"%s",FindInConfig("shader_path4"));

   //settings->paths.video_shader_path3="";
   //settings->paths.video_shader_path3="";
   sprintf(settings->paths.video_shader_path3,"%s","./shaders/shaders_glsl/crt/scanline.glslp");
   sprintf(settings->paths.video_shader_path4,"%s","./shaders/shaders_glsl/crt/fakelottes.glslp");

   printf(settings->paths.video_shader_path1);
   printf("\nKey_11111111111111\n");
   printf(settings->paths.video_shader_path2);
   printf("\nKey_11111111111111\n");
   printf(settings->paths.video_shader_path3);
   printf("\nKey_11111111111111\n");
   printf(settings->paths.video_shader_path4);
   printf("\nKey_11111111111111\n");
}
/* TODO/FIXME - global state - perhaps move outside this file */
static path_change_data_t *file_change_data = NULL;

/**
 * fill_pathname_expanded_and_absolute:
 * @param out_path
 * String to write into.
 * @param in_refpath
 * Used to get the base path if in_path is relative.
 * @param in_path
 * Path to turn into an absolute path.
 *
 * Takes a path and returns an absolute path.
 * It will expand it if the path was using the root path format.
 * e.g. :\shaders
 * If the path was relative it will take this path and get the 
 * absolute path using in_refpath as the path to extract a base path.
 *
 * out_path is filled with the absolute path
 **/
static void fill_pathname_expanded_and_absolute(char *out_path,
      const char *in_refpath, const char *in_path)
{
   char expanded_path[PATH_MAX_LENGTH];

   expanded_path[0] = '\0';

   /* Expand paths which start with :\ to an absolute path */
   fill_pathname_expand_special(expanded_path,
         in_path, sizeof(expanded_path));

   /* Resolve the reference path relative to the config */
   if (path_is_absolute(expanded_path))
      strlcpy(out_path, expanded_path, PATH_MAX_LENGTH);
   else
      fill_pathname_resolve_relative(out_path, in_refpath,
            in_path, PATH_MAX_LENGTH);

   pathname_conform_slashes_to_os(out_path);
}

static void gather_reference_path_list(
      struct path_linked_list *in_path_linked_list, 
      char *path, 
      int reference_depth)
{
   config_file_t *conf = NULL;

   if (reference_depth > SHADER_MAX_REFERENCE_DEPTH)
      return;

   if ((conf = config_file_new_from_path_to_string(path)))
   {
      struct path_linked_list *ref_tmp = (struct path_linked_list*)conf->references;
      while (ref_tmp)
      {
         char* reference_preset_path = (char*)malloc(PATH_MAX_LENGTH);

         fill_pathname_expanded_and_absolute(reference_preset_path, conf->path, ref_tmp->path);
         gather_reference_path_list(in_path_linked_list, reference_preset_path, reference_depth + 1);

         free(reference_preset_path);
         ref_tmp = ref_tmp->next;
      }
      path_linked_list_add_path(in_path_linked_list, path);
   }
   else
   {
      RARCH_WARN("\n            [Shaders]: No Preset located at \"%s\".\n", path);
   }
   config_file_free(conf);
}

/**
 * wrap_mode_to_str:
 * @param type
 * Wrap type.
 *
 * Translates wrap mode to human-readable string identifier.
 *
 * @return human-readable string identifier of wrap mode.
 **/
static const char *wrap_mode_to_str(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
         return "clamp_to_border";
      case RARCH_WRAP_EDGE:
         return "clamp_to_edge";
      case RARCH_WRAP_REPEAT:
         return "repeat";
      case RARCH_WRAP_MIRRORED_REPEAT:
         return "mirrored_repeat";
      default:
         break;
   }

   return "???";
}

/**
 * wrap_str_to_mode:
 * @param type
 * Wrap type in human-readable string format.
 *
 * Translates wrap mode from human-readable string to enum mode value.
 *
 * @return enum mode value of wrap type.
 **/
static enum gfx_wrap_type wrap_str_to_mode(const char *wrap_mode)
{
   if (string_is_equal(wrap_mode,      "clamp_to_border"))
      return RARCH_WRAP_BORDER;
   else if (string_is_equal(wrap_mode, "clamp_to_edge"))
      return RARCH_WRAP_EDGE;
   else if (string_is_equal(wrap_mode, "repeat"))
      return RARCH_WRAP_REPEAT;
   else if (string_is_equal(wrap_mode, "mirrored_repeat"))
      return RARCH_WRAP_MIRRORED_REPEAT;

   RARCH_WARN("[Shaders]: Invalid wrapping type \"%s\". Valid ones are: \"clamp_to_border\" "
         "(default), \"clamp_to_edge\", \"repeat\" and \"mirrored_repeat\". Falling back to default.\n",
         wrap_mode);
   return RARCH_WRAP_DEFAULT;
}

/**
 * video_shader_parse_pass:
 * @param conf
 * Preset file to read from.
 * @param pass
 * Shader passes handle.
 * @param i
 * Index of shader pass.
 *
 * Parses shader pass from preset file.
 *
 * @return true if successful, otherwise false.
 **/
static bool video_shader_parse_pass(config_file_t *conf,
      struct video_shader_pass *pass, unsigned i)
{
   char shader_name[64];
   char filter_name_buf[64];
   char wrap_name_buf[64];
   char frame_count_mod_buf[64];
   char srgb_output_buf[64];
   char fp_fbo_buf[64];
   char mipmap_buf[64];
   char alias_buf[64];
   char scale_name_buf[64];
   char attr_name_buf[64];
   char scale_type[64];
   char scale_type_x[64];
   char scale_type_y[64];
   char formatted_num[8];
   char tmp_path[PATH_MAX_LENGTH];
   struct gfx_fbo_scale *scale      = NULL;
   bool tmp_bool                    = false;
   struct config_entry_list *entry  = NULL;

   fp_fbo_buf[0]      = mipmap_buf[0]          = alias_buf[0]       =
   scale_name_buf[0]  = attr_name_buf[0]       = scale_type[0]      =
   scale_type_x[0]    = scale_type_y[0]        =
   shader_name[0]     = filter_name_buf[0]     = wrap_name_buf[0]   = 
                        frame_count_mod_buf[0] = srgb_output_buf[0] = '\0';
   formatted_num[0]                                                 = '\0';

   snprintf(formatted_num, sizeof(formatted_num), "%u", i);

   /* Source */
   strlcpy(shader_name, "shader",      sizeof(shader_name));
   strlcat(shader_name, formatted_num, sizeof(shader_name));
   if (!config_get_path(conf, shader_name, tmp_path, sizeof(tmp_path)))
   {
      RARCH_ERR("[Shaders]: Couldn't parse shader source \"%s\".\n", shader_name);
      return false;
   }

   /* Get the absolute path */
   fill_pathname_expanded_and_absolute(pass->source.path,
         conf->path, tmp_path);

   /* Smooth */
   strlcpy(filter_name_buf, "filter_linear", sizeof(filter_name_buf));
   strlcat(filter_name_buf, formatted_num,   sizeof(filter_name_buf));

   if (config_get_bool(conf, filter_name_buf, &tmp_bool))
   {
      bool smooth  = tmp_bool;
      pass->filter = smooth ? RARCH_FILTER_LINEAR : RARCH_FILTER_NEAREST;
   }
   else
      pass->filter = RARCH_FILTER_UNSPEC;

   /* Wrapping mode */
   strlcpy(wrap_name_buf,   "wrap_mode",   sizeof(wrap_name_buf));
   strlcat(wrap_name_buf, formatted_num,   sizeof(wrap_name_buf));
   if ((entry = config_get_entry(conf, wrap_name_buf)) 
         && !string_is_empty(entry->value))
      pass->wrap = wrap_str_to_mode(entry->value);
   entry = NULL;

   /* Frame count mod */
   strlcpy(frame_count_mod_buf, "frame_count_mod",   sizeof(frame_count_mod_buf));
   strlcat(frame_count_mod_buf, formatted_num,       sizeof(frame_count_mod_buf));
   if ((entry = config_get_entry(conf, frame_count_mod_buf)) 
         && !string_is_empty(entry->value))
      pass->frame_count_mod = (unsigned)strtoul(entry->value, NULL, 0);
   entry = NULL;

   /* FBO types and mipmapping */
   strlcpy(srgb_output_buf, "srgb_framebuffer", sizeof(srgb_output_buf));
   strlcat(srgb_output_buf, formatted_num,      sizeof(srgb_output_buf));
   if (config_get_bool(conf, srgb_output_buf, &tmp_bool))
      pass->fbo.srgb_fbo = tmp_bool;

   strlcpy(fp_fbo_buf, "float_framebuffer", sizeof(fp_fbo_buf));
   strlcat(fp_fbo_buf, formatted_num,       sizeof(fp_fbo_buf));
   if (config_get_bool(conf, fp_fbo_buf, &tmp_bool))
      pass->fbo.fp_fbo = tmp_bool;

   strlcpy(mipmap_buf, "mipmap_input",      sizeof(mipmap_buf));
   strlcat(mipmap_buf, formatted_num,       sizeof(mipmap_buf));
   if (config_get_bool(conf, mipmap_buf, &tmp_bool))
      pass->mipmap = tmp_bool;

   strlcpy(alias_buf, "alias",        sizeof(alias_buf));
   strlcat(alias_buf, formatted_num,  sizeof(alias_buf));
   if (!config_get_array(conf, alias_buf, pass->alias, sizeof(pass->alias)))
      *pass->alias = '\0';

   /* Scale */
   scale = &pass->fbo;
   strlcpy(scale_name_buf, "scale_type",   sizeof(scale_name_buf));
   strlcat(scale_name_buf, formatted_num,  sizeof(scale_name_buf));
   config_get_array(conf, scale_name_buf, scale_type, sizeof(scale_type));

   strlcpy(scale_name_buf, "scale_type_x",   sizeof(scale_name_buf));
   strlcat(scale_name_buf, formatted_num,    sizeof(scale_name_buf));
   config_get_array(conf, scale_name_buf, scale_type_x, sizeof(scale_type_x));

   strlcpy(scale_name_buf, "scale_type_y",   sizeof(scale_name_buf));
   strlcat(scale_name_buf, formatted_num,    sizeof(scale_name_buf));
   config_get_array(conf, scale_name_buf, scale_type_y, sizeof(scale_type_y));

   if (*scale_type)
   {
      strlcpy(scale_type_x, scale_type, sizeof(scale_type_x));
      strlcpy(scale_type_y, scale_type, sizeof(scale_type_y));
   }
   else if (!*scale_type_x && !*scale_type_y)
      return true;

   scale->valid   = true;
   scale->type_x  = RARCH_SCALE_INPUT;
   scale->type_y  = RARCH_SCALE_INPUT;
   scale->scale_x = 1.0;
   scale->scale_y = 1.0;

   if (*scale_type_x)
   {
      if (string_is_equal(scale_type_x, "source"))
         scale->type_x = RARCH_SCALE_INPUT;
      else if (string_is_equal(scale_type_x, "viewport"))
         scale->type_x = RARCH_SCALE_VIEWPORT;
      else if (string_is_equal(scale_type_x, "absolute"))
         scale->type_x = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("[Shaders]: Invalid attribute: \"%s\".\n", scale_type_x);
         return false;
      }
   }

   if (*scale_type_y)
   {
      if (string_is_equal(scale_type_y, "source"))
         scale->type_y = RARCH_SCALE_INPUT;
      else if (string_is_equal(scale_type_y, "viewport"))
         scale->type_y = RARCH_SCALE_VIEWPORT;
      else if (string_is_equal(scale_type_y, "absolute"))
         scale->type_y = RARCH_SCALE_ABSOLUTE;
      else
      {
         RARCH_ERR("[Shaders]: Invalid attribute: \"%s\".\n", scale_type_y);
         return false;
      }
   }

   strlcpy(attr_name_buf, "scale",        sizeof(attr_name_buf));
   strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));

   if (scale->type_x == RARCH_SCALE_ABSOLUTE)
   {
      int iattr          = 0;
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_x    = iattr;
      else
      {
	      strlcpy(attr_name_buf, "scale_x",      sizeof(attr_name_buf));
	      strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));
         if (config_get_int(conf, attr_name_buf, &iattr))
            scale->abs_x = iattr;
      }
   }
   else
   {
      float fattr          = 0.0f;
      if (config_get_float(conf, attr_name_buf, &fattr))
         scale->scale_x    = fattr;
      else
      {
	      strlcpy(attr_name_buf, "scale_x",      sizeof(attr_name_buf));
	      strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_x = fattr;
      }
   }

   strlcpy(attr_name_buf, "scale",        sizeof(attr_name_buf));
   strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));

   if (scale->type_y == RARCH_SCALE_ABSOLUTE)
   {
      int iattr          = 0;
      if (config_get_int(conf, attr_name_buf, &iattr))
         scale->abs_y    = iattr;
      else
      {
         strlcpy(attr_name_buf, "scale_y",      sizeof(attr_name_buf));
         strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));
         if (config_get_int(conf, attr_name_buf, &iattr))
            scale->abs_y = iattr;
      }
   }
   else
   {
      float fattr          = 0.0f;
      if (config_get_float(conf, attr_name_buf, &fattr))
         scale->scale_y    = fattr;
      else
      {
         strlcpy(attr_name_buf, "scale_y",      sizeof(attr_name_buf));
         strlcat(attr_name_buf, formatted_num,  sizeof(attr_name_buf));
         if (config_get_float(conf, attr_name_buf, &fattr))
            scale->scale_y = fattr;
      }
   }

   return true;
}

/**
 * video_shader_parse_textures:
 * @param conf
 * Preset file to read from.
 * @param shader
 * Shader pass handle.
 *
 * Parses shader textures.
 *
 * @return true if successful, otherwise false.
 **/
static bool video_shader_parse_textures(config_file_t *conf,
      struct video_shader *shader)
{
   size_t path_size     = PATH_MAX_LENGTH;
   const char *id       = NULL;
   char *save           = NULL;
   char *textures       = (char*)malloc(1024 + path_size);
   char texture_path[PATH_MAX_LENGTH];

   if (!textures)
      return false;

   textures[0] = '\0';
   texture_path[0] = '\0';

   if (!config_get_array(conf, "textures", textures, 1024))
   {
      free(textures);
      return true;
   }

   for (id = strtok_r(textures, ";", &save);
         id && shader->luts < GFX_MAX_TEXTURES;
         shader->luts++, id = strtok_r(NULL, ";", &save))
   {
      char id_filter[64];
      char id_wrap[64];
      char id_mipmap[64];
      bool mipmap         = false;
      bool smooth         = false;
      struct config_entry_list 
         *entry           = NULL;

      id_filter[0] = id_wrap[0] = id_mipmap[0] = '\0';

      if (!(entry = config_get_entry(conf, id)) ||
            string_is_empty(entry->value))
      {
         RARCH_ERR("[Shaders]: Cannot find path to texture \"%s\"..\n",
               id);
         free(textures);
         return false;
      }

      config_get_path(conf, id, texture_path, sizeof(texture_path));

      /* Get the absolute path */
      fill_pathname_expanded_and_absolute(
            shader->lut[shader->luts].path, conf->path, texture_path);

      entry = NULL;

      strlcpy(shader->lut[shader->luts].id, id,
            sizeof(shader->lut[shader->luts].id));

      strlcpy(id_filter, id, sizeof(id_filter));
      strlcat(id_filter, "_linear", sizeof(id_filter));
      if (config_get_bool(conf, id_filter, &smooth))
         shader->lut[shader->luts].filter = smooth 
            ? RARCH_FILTER_LINEAR 
            : RARCH_FILTER_NEAREST;
      else
         shader->lut[shader->luts].filter = RARCH_FILTER_UNSPEC;

      strlcpy(id_wrap, id,           sizeof(id_wrap));
      strlcat(id_wrap, "_wrap_mode", sizeof(id_wrap));
      if ((entry = config_get_entry(conf, id_wrap)) 
            && !string_is_empty(entry->value))
         shader->lut[shader->luts].wrap = wrap_str_to_mode(entry->value);
      entry = NULL;

      strlcpy(id_mipmap, id,        sizeof(id_mipmap));
      strlcat(id_mipmap, "_mipmap", sizeof(id_mipmap));
      if (config_get_bool(conf, id_mipmap, &mipmap))
         shader->lut[shader->luts].mipmap = mipmap;
      else
         shader->lut[shader->luts].mipmap = false;
   }

   free(textures);
   return true;
}

/**
 * video_shader_parse_find_parameter:
 * @param params
 * Shader parameter handle.
 * @param num_params
 * Number of shader params in @params.
 * param id
 * Identifier to search for.
 *
 * Finds a shader parameter with identifier @id in @params..
 *
 * @return Handle to shader parameter if successful, otherwise NULL.
 **/
static struct video_shader_parameter *video_shader_parse_find_parameter(
      struct video_shader_parameter *params,
      unsigned num_params, const char *id)
{
   unsigned i;

   for (i = 0; i < num_params; i++)
   {
      if (string_is_equal(params[i].id, id))
         return &params[i];
   }

   return NULL;
}

/**
 * video_shader_resolve_parameters:
 * @conf              : Preset file to read from.
 * @shader            : Shader passes handle.
 *
 * Resolves all shader parameters belonging to shaders
 * from the #pragma parameter lines in the shader for each pass.
 * 
 **/
void video_shader_resolve_parameters(struct video_shader *shader)
{
   unsigned i;
   struct video_shader_parameter *param = &shader->parameters[0];

   shader->num_parameters = 0;

#ifdef DEBUG
   /* Find all parameters in our shaders. */
   RARCH_DBG("[Shaders]: Finding parameters in shader passes (#pragma parameter)..\n");
#endif

   for (i = 0; i < shader->passes; i++)
   {
      const char *path          = shader->pass[i].source.path;
      uint8_t *buf              = NULL;
      int64_t buf_len           = 0;

      if (string_is_empty(path))
         continue;

      if (!path_is_valid(path))
         continue;

      /* First try to use the more robust slang implementation 
       * to support #includes. */

#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      /* FIXME: The check for slang can be removed
       * if it's sufficiently tested for GLSL/Cg as well, 
       * it should be the same implementation. 
       * The problem with switching currently is that it looks 
       * for a #version string in the first line of the file 
       * which glsl doesn't have */

      if (     string_is_equal(path_get_extension(path), "slang") 
            && slang_preprocess_parse_parameters(path, shader))
         continue;
#endif

      /* Read file contents */
      if (filestream_read_file(path, (void**)&buf, &buf_len))
      {
         size_t line_index         = 0;
         struct string_list lines  = {0};
         bool lines_inited         = false;

         /* Split into lines */
         if (buf_len > 0)
         {
            string_list_initialize(&lines);
            lines_inited = string_split_noalloc(&lines, (const char*)buf, "\n");
         }

         /* Buffer is no longer required - clean up */
         if ((void*)buf)
            free((void*)buf);

         if (!lines_inited)
            continue;

         /* Even though the pass is set in the loop too, 
          * not all passes have parameters */
         param->pass = i;

         while ((shader->num_parameters < ARRAY_SIZE(shader->parameters)) 
               && (line_index < lines.size))
         {
            int ret;
            const char *line = lines.elems[line_index].data;
            line_index++;

            /* Check if this is a '#pragma parameter' line */
            if (strncmp("#pragma parameter", line,
                     STRLEN_CONST("#pragma parameter")))
               continue;

            /* Parse line */
            if ((ret = sscanf(line, "#pragma parameter %63s \"%63[^\"]\" %f %f %f %f",
                  param->id,        param->desc,    &param->initial,
                  &param->minimum, &param->maximum, &param->step)) < 5)
               continue;

            param->id[63]   = '\0';
            param->desc[63] = '\0';

            if (ret == 5)
               param->step  = 0.1f * (param->maximum - param->minimum);

            param->pass     = i;

#ifdef DEBUG
            RARCH_DBG("[Shaders]: Found #pragma parameter %s (%s) %f %f %f %f in pass %d.\n",
                  param->desc,    param->id,      param->initial,
                  param->minimum, param->maximum, param->step, param->pass);
#endif
            param->current  = param->initial;

            shader->num_parameters++;
            param++;
         }

         string_list_deinitialize(&lines);
      }
   }
}


/**
 * video_shader_load_current_parameter_values:
 * @param conf
 * Preset file to read from.
 * @param shader
 * Shader passes handle.
 *
 * For each parameter in the shader, if a value is set in the config file
 * load this value to the parameter's current value.
 *
 * @return true (1) if successful, otherwise false (0).
 **/
bool video_shader_load_current_parameter_values(
      config_file_t *conf, struct video_shader *shader)
{
   unsigned i;

   if (!conf)
      return false;

   /* For all parameters in the shader see if there is any config value set */
   for (i = 0; i < shader->num_parameters; i++)
   {
      const struct config_entry_list *entry = config_get_entry(conf, shader->parameters[i].id);
      
      /* Only try to load the parameter value if an entry exists in the config */
      if (entry)
      {
         struct video_shader_parameter *parameter = (struct video_shader_parameter*)
            video_shader_parse_find_parameter(shader->parameters, 
                  shader->num_parameters, 
                  shader->parameters[i].id);

         /* Log each parameter read */
         if (config_get_float(conf, shader->parameters[i].id, &parameter->current))
            RARCH_DBG("[Shaders]: Load parameter value: \"%s\" = %f.\n", shader->parameters[i].id, parameter->current);
         else
            RARCH_WARN("[Shaders]: Load parameter value: \"%s\" is set in preset but couldn't load its value.\n",
                        shader->parameters[i].id);
      }
   }

   return true;
}

static const char *scale_type_to_str(enum gfx_scale_type type)
{
   switch (type)
   {
      case RARCH_SCALE_INPUT:
         return "source";
      case RARCH_SCALE_VIEWPORT:
         return "viewport";
      case RARCH_SCALE_ABSOLUTE:
         return "absolute";
      default:
         break;
   }

   return "?";
}

static void shader_write_scale_dim(config_file_t *conf,
      const char *dim,
      const char *formatted_num,
      enum gfx_scale_type type, 
      float scale,
      unsigned absolute)
{
   char key[64];
   char dim_str[64];
   strlcpy(dim_str, dim, sizeof(dim_str));
   strlcat(dim_str, formatted_num, sizeof(dim_str));

   strlcpy(key, "scale_type_", sizeof(key));
   strlcat(key, dim_str,       sizeof(key));
   config_set_string(conf, key, scale_type_to_str(type));

   strlcpy(key, "scale_", sizeof(key));
   strlcat(key, dim_str,  sizeof(key));
   if (type == RARCH_SCALE_ABSOLUTE)
      config_set_int(conf, key, absolute);
   else
      config_set_float(conf, key, scale);
}

static void shader_write_fbo(config_file_t *conf,
      const char *formatted_num,
      const struct gfx_fbo_scale *fbo)
{
   char key[64];
   strlcpy(key, "float_framebuffer", sizeof(key));
   strlcat(key, formatted_num,       sizeof(key));
   config_set_string(conf, key, fbo->fp_fbo ? "true" : "false");
   strlcpy(key, "srgb_framebuffer", sizeof(key));
   strlcat(key, formatted_num,      sizeof(key));
   config_set_string(conf, key, fbo->srgb_fbo ? "true" : "false");

   if (!fbo->valid)
      return;

   shader_write_scale_dim(conf, "x", formatted_num, fbo->type_x, fbo->scale_x, fbo->abs_x);
   shader_write_scale_dim(conf, "y", formatted_num, fbo->type_y, fbo->scale_y, fbo->abs_y);
}

/**
 * video_shader_write_root_preset:
 * @conf              : Preset file to write to.
 * @shader            : Shader passes handle.
 * @preset_path       : Optional path to where the preset will be written.
 *
 * Writes preset and all associated state (passes, textures, imports, 
 * etc) into @conf.
 * If @preset_path is not NULL, shader paths are saved relative to it.
 **/
static bool video_shader_write_root_preset(const struct video_shader *shader,
      const char *path)
{
   unsigned i;
   char key[64];
   bool ret             = true;
   char *tmp            = (char*)malloc(3 * PATH_MAX_LENGTH);
   char *tmp_rel        = tmp +     PATH_MAX_LENGTH;
   char *tmp_base       = tmp + 2 * PATH_MAX_LENGTH;
   config_file_t *conf  = NULL;

   if (!(conf = config_file_new_alloc()))
      return false;

   if (!tmp)
   {
      config_file_free(conf);
      return false;
   }

   RARCH_DBG("[Shaders]: Saving full preset to: \"%s\".\n", path);

   config_set_int(conf, "shaders", shader->passes);
   if (shader->feedback_pass >= 0)
      config_set_int(conf, "feedback_pass", shader->feedback_pass);

   strlcpy(tmp_base, path, PATH_MAX_LENGTH);

   /* ensure we use a clean base like the shader passes and texture paths do */
   path_resolve_realpath(tmp_base, PATH_MAX_LENGTH, false);
   path_basedir(tmp_base);

   for (i = 0; i < shader->passes; i++)
   {
      char formatted_num[8];
      const struct video_shader_pass *pass = &shader->pass[i];

      formatted_num[0]                     = '\0';

      snprintf(formatted_num, sizeof(formatted_num), "%u", i);

      strlcpy(key, "shader",      sizeof(key));
      strlcat(key, formatted_num, sizeof(key));

      strlcpy(tmp, pass->source.path, PATH_MAX_LENGTH);
      path_relative_to(tmp_rel, tmp, tmp_base, PATH_MAX_LENGTH);

      pathname_make_slashes_portable(tmp_rel);

      config_set_path(conf, key, tmp_rel);

      if (pass->filter != RARCH_FILTER_UNSPEC)
      {
         strlcpy(key, "filter_linear", sizeof(key));
         strlcat(key, formatted_num,   sizeof(key));
         config_set_string(conf, key,
               (pass->filter == RARCH_FILTER_LINEAR)
               ? "true"
               : "false");
      }

      strlcpy(key, "wrap_mode",   sizeof(key));
      strlcat(key, formatted_num, sizeof(key));
      config_set_string(conf, key, wrap_mode_to_str(pass->wrap));

      if (pass->frame_count_mod)
      {
         strlcpy(key, "frame_count_mod", sizeof(key));
         strlcat(key, formatted_num,     sizeof(key));
         config_set_int(conf, key, pass->frame_count_mod);
      }

      strlcpy(key, "mipmap_input", sizeof(key));
      strlcat(key, formatted_num,  sizeof(key));
      config_set_string(conf, key, pass->mipmap ? "true" : "false");

      strlcpy(key, "alias",       sizeof(key));
      strlcat(key, formatted_num, sizeof(key));
      config_set_string(conf, key, pass->alias);

      shader_write_fbo(conf, formatted_num, &pass->fbo);
   }

   /* Write shader parameters which are different than the default shader values */
   if (shader->num_parameters)
      for (i = 0; i < shader->num_parameters; i++)
         if (shader->parameters[i].current != shader->parameters[i].initial)
            config_set_float(conf, shader->parameters[i].id, shader->parameters[i].current);

   if (shader->luts)
   {
      char textures[4096];

      textures[0] = '\0';

      /* Names of the textures */
      strlcpy(textures, shader->lut[0].id, sizeof(textures));

      for (i = 1; i < shader->luts; i++)
      {
         /* O(n^2), but number of textures is very limited. */
         strlcat(textures, ";", sizeof(textures));
         strlcat(textures, shader->lut[i].id, sizeof(textures));
      }

      config_set_string(conf, "textures", textures);

      /* Step through the textures in the shader */
      for (i = 0; i < shader->luts; i++)
      {
         fill_pathname_abbreviated_or_relative(tmp_rel,
               tmp_base, shader->lut[i].path, PATH_MAX_LENGTH);
         pathname_make_slashes_portable(tmp_rel);
         config_set_string(conf, shader->lut[i].id, tmp_rel);

         /* Linear filter ON or OFF */
         if (shader->lut[i].filter != RARCH_FILTER_UNSPEC)
         {
            char k[128];
            strlcpy(k, shader->lut[i].id, sizeof(k));
            strlcat(k, "_linear",         sizeof(k));
            config_set_string(conf, k, 
                  (shader->lut[i].filter == RARCH_FILTER_LINEAR)
                  ? "true"
                  : "false");
         }

         /* Wrap Mode */
         {
            char k[128];
            strlcpy(k, shader->lut[i].id, sizeof(k));
            strlcat(k, "_wrap_mode",      sizeof(k));
            config_set_string(conf, k,
                  wrap_mode_to_str(shader->lut[i].wrap));
         }

         /* Mipmap On or Off */
         {
            char k[128];
            strlcpy(k, shader->lut[i].id, sizeof(k));
            strlcat(k, "_mipmap",         sizeof(k));
            config_set_string(conf, k, shader->lut[i].mipmap
                  ? "true"
                  : "false");
         }
      }
   }

   /* Write the File! */
   ret = config_file_write(conf, path, false);

   config_file_free(conf);
   free(tmp);

   return ret;
}

static config_file_t *video_shader_get_root_preset_config(const char *path)
{
   #if !OLD_CPP
   config_file_t *conf=NULL;
   settings_t *settings= config_get_ptr();
   read_cfg_settting();

   char conf_path[PATH_MAX_LENGTH];
   char app_path[PATH_MAX_LENGTH]         = {0};
#if WIN32
      fill_pathname_home_dir(app_path, sizeof(app_path));
#else
      fill_pathname_application_dir(app_path, sizeof(app_path));
#endif
  /* if (!bIsKey)
   {
     settings->uints.video_shader_type=0;
   }*/

   printf("Key_2222222222222\n");

   if (settings->uints.video_shader_type==1)
   {
      if(settings->uints.video_shader_level==1)
      {
         if(settings->paths.video_shader_path1==NULL)
            return NULL;
          //printf("111");
         printf("滤镜：%s\n", settings->paths.video_shader_path1);

      fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path1, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==2)
      {
         if(settings->paths.video_shader_path2==NULL)
            return NULL;
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path2, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==3)
      {
         if(settings->paths.video_shader_path3==NULL)
            return NULL;
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path3, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==4)
      {
         if(settings->paths.video_shader_path4==NULL)
            return NULL;
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path4, sizeof(conf_path));
      }

      if (!(conf=config_file_new_from_path_to_string(conf_path)))
      {
         printf("shader_error");
         return NULL;
      }
      else
      {
         printf("\nshader:");
         printf(conf_path);
         printf("\n");
      }
      
   }
   return conf;
#else
int reference_depth           = 1;
   char* nested_reference_path   = NULL;
   config_file_t *conf           = config_file_new_from_path_to_string(path);

   if (!conf)
      return NULL;

   nested_reference_path         = (char*)malloc(PATH_MAX_LENGTH);

   while (conf->references)
   {
      /* If we have reached the max depth of nested references,
       * stop attempting to read the next reference,
       * because we are likely in a self referential loop. 
       *
       * SHADER_MAX_REFERENCE_DEPTH references deep seems 
       * like more than enough depth for expected usage */
      if (reference_depth > SHADER_MAX_REFERENCE_DEPTH)
      {
         RARCH_ERR("[Shaders]: Get root preset - Exceeded maximum reference depth (%u) without finding a full preset. "
               "This chain of referenced presets is likely cyclical.\n", SHADER_MAX_REFERENCE_DEPTH);
         config_file_free(conf);
         conf = NULL;
         free(nested_reference_path);
         return NULL;
      }

      /* Get the absolute path for the reference */
      fill_pathname_expanded_and_absolute(nested_reference_path, conf->path, conf->references->path);

      /* Create a new config from the referenced path */
      config_file_free(conf);

      /* If we can't read the reference preset */
      if (!(conf = config_file_new_from_path_to_string(nested_reference_path)))
      {
         RARCH_WARN("[Shaders]: Could not read shader preset in #reference line: \"%s\".\n", nested_reference_path);
         free(nested_reference_path);
         return NULL;
      }

      reference_depth += 1;
   }

   free(nested_reference_path);

   return conf;
#endif
}

/**
 * video_shader_check_reference_chain:
 * @param path_to_save
 * Path of the preset we want to validate is safe to save as 
 * a simple preset.
 * @param reference_path
 * Path of the reference which we would want to write into 
 * the new preset.
 * 
 * Checks to see if we can save a valid simple preset 
 * (preset with a #reference in it) to this path.
 * 
 * This takes into account reference links which can't be 
 * loaded and if saving this file would create a creating 
 * circular reference chain, because some link in
 * the chain references the file path we want to save to.
 * 
 * Checks each preset in the chain of presets with #reference
 * Starts with reference_path. If it has no reference, then 
 * our check is valid.
 * If it has a #reference, then check that the reference path 
 * is not the same as path_to_save.

 * If it is not the same path, then go the the next nested reference
 * 
 * Continues this until it finds a preset without #reference in it, 
 * or it hits the maximum recursion depth (at that point
 * it is probably in a self referential cycle)
 * 
 * @return true (1) if it was able to load all presets and 
 * found a full preset, otherwise false (0).
 **/
static bool video_shader_check_reference_chain_for_save(
      const char *path_to_save, const char *ref_path)
{
   config_file_t *conf    = config_file_new_from_path_to_string(ref_path);
   bool ret                = true;

   if (!conf)
   {
      RARCH_ERR("[Shaders]: Could not read the #reference preset: \"%s\".\n", ref_path);
      return false;
   }
   else
   {
      int ref_depth                 = 1;
      char *path_to_save_conformed  = (char*)malloc(PATH_MAX_LENGTH);
      char *nested_ref_path         = (char*)malloc(PATH_MAX_LENGTH);
      strlcpy(path_to_save_conformed, path_to_save, PATH_MAX_LENGTH);
      pathname_conform_slashes_to_os(path_to_save_conformed);

      while (conf->references)
      {
         /* If we have reached the max depth of nested references stop attempting to read 
          * the next reference because we are likely in a self referential loop. */
         if (ref_depth > SHADER_MAX_REFERENCE_DEPTH)
         {
            RARCH_ERR("[Shaders]: Check reference chain for save - Exceeded maximum reference depth(%u) without "
                      "finding a full preset. This chain of referenced presets is likely cyclical.\n", SHADER_MAX_REFERENCE_DEPTH);
            ret = false;
            break;
         }

         /* Get the absolute path for the reference */
         fill_pathname_expanded_and_absolute(nested_ref_path, conf->path, conf->references->path);

         /* If one of the reference paths is the same as the file we want to save then this reference chain would be 
          * self-referential / cyclical and we can't save this as a simple preset*/
         if (string_is_equal(nested_ref_path, path_to_save_conformed))
         {
            RARCH_WARN("[Shaders]: Saving preset:\n"
                       "        \"%s\"\n"
                       "        With a #reference of:\n"
                       "        \"%s\"\n"
                       "        Would create a cyclical reference in preset:\n"
                       "        \"%s\"\n"
                       "        Which already references preset:\n"
                       "        \"%s\"\n",
                       path_to_save_conformed, ref_path,
                       conf->path, nested_ref_path);
            ret = false;
            break;
         }

         /* Create a new config from the referenced path */
         config_file_free(conf);

         /* If we can't read the reference preset */
         if (!(conf = config_file_new_from_path_to_string(nested_ref_path)))
         {
            RARCH_WARN("[Shaders]: Could not read shader preset "
                  "in #reference line: \"%s\".\n", nested_ref_path);
            ret = false;
            break;
         }

         ref_depth += 1;
      }

      free(path_to_save_conformed);
      free(nested_ref_path);
   }

   config_file_free(conf);

   return ret;
}

/**
 * video_shader_write_referenced_preset:
 * @param path
 * File to write to
 * @param shader
 * Shader preset to write
 *
 * Writes a referenced preset to disk
 * A referenced preset is a preset which includes the #reference directive
 * as it's first line to specify a root preset and can also 
 * include parameter and texture values to override the values 
 * of the root preset
 *
 * @return false if a referenced preset cannot be saved
 **/
static bool video_shader_write_referenced_preset(
      const char *path_to_save,
      const char *shader_dir,
      const struct video_shader *shader)
{
   unsigned i;
   config_file_t *conf                    = NULL;
   config_file_t *ref_conf                = NULL;
   struct video_shader *ref_shader        = (struct video_shader*)
      calloc(1, sizeof(*ref_shader));
   bool ret                               = false;
   bool continue_saving_ref               = true;
   char *new_preset_basedir               = strdup(path_to_save);
   char *config_dir                       = (char*)malloc(PATH_MAX_LENGTH);
   char *relative_tmp_ref_path            = (char*)malloc(PATH_MAX_LENGTH);
   char *abs_tmp_ref_path                 = (char*)malloc(PATH_MAX_LENGTH);
   char *path_to_ref                      = (char*)malloc(PATH_MAX_LENGTH);
   char* path_to_save_conformed           = (char*)malloc(PATH_MAX_LENGTH);

   strlcpy(path_to_save_conformed, path_to_save, PATH_MAX_LENGTH);
   pathname_conform_slashes_to_os(path_to_save_conformed);

   config_dir[0]                          = '\0';
   relative_tmp_ref_path[0]               = '\0';
   abs_tmp_ref_path[0]                    = '\0';
   path_to_ref[0]                         = '\0';

   path_basedir(new_preset_basedir);

   /* Get the retroarch config dir where the automatically 
    * loaded presets are located
    * and where Save Game Preset, Save Core Preset, 
    * Save Global Preset save to */
   fill_pathname_application_special(config_dir, PATH_MAX_LENGTH,
         APPLICATION_SPECIAL_DIRECTORY_CONFIG);

   /* If there is no initial preset path loaded */
   if (string_is_empty(shader->loaded_preset_path))
   {
      RARCH_WARN("[Shaders]: Saving full preset because the loaded shader "
            "does not have "
            "a path to a previously loaded preset file on disk.\n");
      goto end;
   }
   
   /* If the initial preset loaded is the ever-changing retroarch 
    * preset don't save a reference
    * TODO/FIXME - remove once we don't write this preset anymore */
   if (!strncmp(path_basename_nocompression(shader->loaded_preset_path),
            "retroarch",
            STRLEN_CONST("retroarch")))
   {
      RARCH_WARN("[Shaders]: Saving full preset because "
            "a reference to the "
            "ever-changing retroarch preset can't be saved.\n");
      goto end;
   }

   strlcpy(path_to_ref, shader->loaded_preset_path, PATH_MAX_LENGTH);
   pathname_conform_slashes_to_os(path_to_ref);

   /* Get a config from the file we want to make a reference to */
   /* If the original preset can't be loaded, probably because 
    * it isn't there anymore */
   if (!(ref_conf = config_file_new_from_path_to_string(path_to_ref)))
   {
      RARCH_WARN("[Shaders]: Saving full preset because the initially "
            "loaded preset can't be loaded. "
            "It was likely renamed or deleted.\n");
      goto end;
   }

   /* If we are trying to save on top the path referenced in the 
    * initially loaded preset.
    *
    * E.G. Preset_B references Preset_A, I load Preset_B do some 
    * parameter adjustments, 
    * then I save on top of Preset_A, we want to get a preset 
    * just like the original Preset_A with the new parameter 
    * adjustments.
    *
    * If there is a reference in the initially loaded preset,
    * we should check it against the preset path we are currently 
    * trying to save */
   if (ref_conf->references)
   {
      /* Get the absolute path for the reference */
      fill_pathname_expanded_and_absolute(abs_tmp_ref_path,
            ref_conf->path, ref_conf->references->path);

      pathname_conform_slashes_to_os(abs_tmp_ref_path);

      /* If the reference is the same as the path we are trying to save to 
         then this should be used as the reference to save */
      if (string_is_equal(abs_tmp_ref_path, path_to_save_conformed))
      {
         strlcpy(path_to_ref, abs_tmp_ref_path, PATH_MAX_LENGTH);
         config_file_free(ref_conf);
         ref_conf = config_file_new_from_path_to_string(
               path_to_ref);
      }
   }

   /* 
    * If 
    *    The new preset file we are trying to save is the 
    *    same as the initially loaded preset
    * or
    *    The initially loaded preset was located under the 
    *    retroarch config folder
    *    this means that it was likely saved from inside the retroarch UI
    * Then
    *    We should not save a preset with a reference to the initially loaded
    *    preset file itself, instead we need to save a new preset with 
    *    the same reference as was in the initially loaded preset.
    */

   /* If the reference path is the same as the path we want to save 
    * or the reference path is in the config (auto shader) folder */
   if (string_is_equal(path_to_ref, path_to_save_conformed) 
         || !strncmp(config_dir, path_to_ref, strlen(config_dir)))
   {
      /* If the config from the reference path has a reference in it,
       * we will use this same nested reference for the new preset */
      if (ref_conf->references)
      {
         /* Get the absolute path for the reference */
         fill_pathname_expanded_and_absolute(path_to_ref,
               ref_conf->path, ref_conf->references->path);

         /* If the reference path is also the same as what 
          * we are trying to save 
            This can easily happen
            E.G.
            - Save Preset As
            - Save Game Preset
            - Save Preset As (use same name as first time)
         */
         if (string_is_equal(path_to_ref, path_to_save_conformed))
         {
            config_file_free(ref_conf);
            ref_conf = config_file_new_from_path_to_string(path_to_ref);

            /* If the reference also has a reference inside it */
            /* Get the absolute path for the reference */
            if (ref_conf->references)
               fill_pathname_expanded_and_absolute(path_to_ref,
                     ref_conf->path, ref_conf->references->path);
            else
            {
               /* If the config referenced is a full preset */
               RARCH_WARN("[Shaders]: Saving full preset because "
                     "a preset which "
                     "would reference itself can't be saved.\n");
               goto end;
            }
         }
      }
      /* If there is no reference in the initial preset we need to 
       * save a full preset */
      else
      {
         /* We can't save a reference to ourselves */
         RARCH_WARN("[Shaders]: Saving full preset because "
               "a preset which "
               "would reference itself can't be saved.\n");
         goto end;
      }
   }

   /* Check the reference chain that we would be saving to make sure it 
    * is valid */
   if (!video_shader_check_reference_chain_for_save(
            path_to_save_conformed, path_to_ref))
   {
      RARCH_WARN("[Shaders]: Saving full preset because saving a "
            "simple preset would result "
            "in a cyclical reference, or a preset in the reference "
            "chain could not be read.\n");
      goto end;
   }

   RARCH_DBG("[Shaders]: Reading preset to compare with "
         "current values: \"%s\".\n", path_to_save_conformed);

   /* Load the preset referenced in the preset into the shader */
   if (!video_shader_load_preset_into_shader(path_to_ref, ref_shader))
   {
      RARCH_WARN("[Shaders]: Saving full preset because "
            "the preset could not be loaded from #reference line: \"%s\".\n",
            path_to_ref);
      goto end;
   }

   /* Create a new EMPTY config */
   if (!(conf = config_file_new_alloc()))
      goto end;

   conf->path = strdup(path_to_save_conformed);

   pathname_make_slashes_portable(relative_tmp_ref_path);

   /* Add the reference path to the config */
   config_file_add_reference(conf, path_to_ref);

   /* Set modified to true so when you run config_file_write 
    * it will save a file */
   conf->modified = true;

   /* 
      Compare the shader to a shader created from the referenced 
      config to see if we can save a referenced preset and what 
      parameters and textures of the root_config are overridden
   */

   /* Check number of passes match */
   if (shader->passes != ref_shader->passes)
   {
      RARCH_WARN("[Shaders]: Passes (number of passes) "
                  "- Current value doesn't match referenced value "
                  "- Full preset will be saved instead of simple preset.\n");
      continue_saving_ref = false;
   }

   /* Compare all passes from the shader, if anything is different 
    * then we should not save a reference and instead save a 
    * full preset instead.
   */
   if (continue_saving_ref)
   {
      /* Step through each pass comparing all the properties to 
       * make sure they match */
      for (i = 0; (i < shader->passes && continue_saving_ref);
            i++)
      {
         const struct video_shader_pass *pass      = &shader->pass[i];
         const struct video_shader_pass *root_pass = &ref_shader->pass[i];
         const struct gfx_fbo_scale *fbo           = &pass->fbo;
         const struct gfx_fbo_scale *root_fbo      = &root_pass->fbo;

         if (!string_is_equal(pass->source.path, root_pass->source.path))
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u path", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && pass->filter != root_pass->filter)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u filter", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && pass->wrap != root_pass->wrap)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u wrap", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && pass->frame_count_mod != root_pass->frame_count_mod)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u frame_count", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && pass->mipmap != root_pass->mipmap)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u mipmap", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && !string_is_equal(pass->alias, root_pass->alias))
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u alias", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->type_x != root_fbo->type_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u type_x", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->type_y != root_fbo->type_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u type_y", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->scale_x != root_fbo->scale_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u scale_x", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->scale_y != root_fbo->scale_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u scale_y", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->fp_fbo != root_fbo->fp_fbo)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u fp_fbo", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->srgb_fbo != root_fbo->srgb_fbo)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u srgb_fbo", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->valid != root_fbo->valid)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u valid", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->abs_x != root_fbo->abs_x)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u abs_x", i);
#endif
            continue_saving_ref = false;
         }

         if (continue_saving_ref && fbo->abs_y != root_fbo->abs_y)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Pass %u abs_y", i);
#endif
            continue_saving_ref = false;
         }

         if (!continue_saving_ref)
         {
#ifdef DEBUG
            RARCH_WARN("[Shaders]: Current value doesn't match referenced value "
                  "- Full preset will be saved instead of simple preset.\n");
#endif
            goto end;
         }
      }
   }

   /* If the shader has parameters */
   if (shader->num_parameters)
   {
      for (i = 0; i < shader->num_parameters; i++)
      {
         /* If the parameter's current value is different 
          * than the referenced shader then write the value 
          * into the new preset */
         if (     shader->parameters[i].current 
               != ref_shader->parameters[i].current)
            config_set_float(conf, shader->parameters[i].id,
                  shader->parameters[i].current);
      }
   }

   /* If the shader has textures */
   if (shader->luts)
   {
      for (i = 0; i < shader->luts; i++)
      {
         /* If the current shader texture path is different 
          * than the referenced shader texture then write the 
          * current path into the new preset */
         if (!string_is_equal(ref_shader->lut[i].path,
                  shader->lut[i].path))
         {
            char *path_for_save  = (char*)malloc(PATH_MAX_LENGTH);

            fill_pathname_abbreviated_or_relative(path_for_save,
                  conf->path, shader->lut[i].path, PATH_MAX_LENGTH);
            pathname_make_slashes_portable(path_for_save);
            config_set_string(conf, shader->lut[i].id, path_for_save);
#ifdef DEBUG
            RARCH_DBG("[Shaders]: Texture override \"%s\" = \"%s\".\n",
                  shader->lut[i].id, path_for_save);
#endif

            free(path_for_save);
         }
      }
   }

   /* Write the file, return will be true if successful */
   RARCH_DBG("[Shaders]: Saving simple preset to: \"%s\"\n",
         path_to_save_conformed);
   ret = config_file_write(conf, path_to_save_conformed, false);

end:

   config_file_free(conf);
   config_file_free(ref_conf);
   free(ref_shader);
   free(abs_tmp_ref_path);
   free(relative_tmp_ref_path);
   free(new_preset_basedir);
   free(config_dir);
   free(path_to_ref);
   free(path_to_save_conformed);

   return ret;
}

/**
 * video_shader_load_root_config_into_shader:
 * @param conf
 * Preset file to read from.
 * @param shader
 * Shader handle.
 *
 * Loads preset file and all associated state 
 * (passes, textures, imports, etc).
 *
 * @return true (1) if successful, otherwise false (0).
 **/
static bool video_shader_load_root_config_into_shader(
      config_file_t *conf, 
      settings_t *settings,
      struct video_shader *shader)
{
   unsigned i;
   unsigned num_passes = 0;
   bool watch_files    = settings->bools.video_shader_watch_files;

   /* This sets the shader to empty */
   memset(shader, 0, sizeof(*shader));

   if (!config_get_uint(conf, "shaders", &num_passes))
      return false;
   if (!num_passes)
      return false;

   if (!config_get_int(conf, "feedback_pass", &shader->feedback_pass))
      shader->feedback_pass = -1;

   shader->passes = MIN(num_passes, GFX_MAX_SHADERS);

   /* Set the path of the root preset for this shader */
   strlcpy(shader->path, conf->path, sizeof(shader->path));

   /* Set the path of the original preset which was loaded, for 
    * a full preset config this is the same as the root config 
    * For simple presets (using #reference) this different than 
    * the root preset and it is the path to the 
    * simple preset originally loaded, but that is set inside 
    * video_shader_load_preset_into_shader*/
   strlcpy(shader->loaded_preset_path, 
         conf->path,
         sizeof(shader->loaded_preset_path));

   if (watch_files)
   {
      union string_list_elem_attr attr;
      int flags                        = 
         PATH_CHANGE_TYPE_MODIFIED                   |
         PATH_CHANGE_TYPE_WRITE_FILE_CLOSED          |
         PATH_CHANGE_TYPE_FILE_MOVED                 |
         PATH_CHANGE_TYPE_FILE_DELETED;
      struct string_list file_list     = {0};

      attr.i         = 0;

      if (file_change_data)
         frontend_driver_watch_path_for_changes(NULL, 0, &file_change_data);

      file_change_data = NULL;
      string_list_initialize(&file_list);
      string_list_append(&file_list, conf->path, attr);

      /* TODO We aren't currently watching the originally loaded preset
       * We should probably watch it for changes too */

      for (i = 0; i < shader->passes; i++)
      {
         if (!video_shader_parse_pass(conf, &shader->pass[i], i))
         {
            string_list_deinitialize(&file_list);
            return false;
         }

         string_list_append(&file_list, shader->pass[i].source.path, attr);
      }

      frontend_driver_watch_path_for_changes(&file_list, flags,
            &file_change_data);
      string_list_deinitialize(&file_list);
   }
   else
   {
      for (i = 0; i < shader->passes; i++)
      {
         if (!video_shader_parse_pass(conf, &shader->pass[i], i))
            return false;
      }
   }

   if (!video_shader_parse_textures(conf, shader))
      return false;

   /* Load the parameter values */
   video_shader_resolve_parameters(shader);

   /* Load the parameter values */
   video_shader_load_current_parameter_values(conf, shader);

#ifdef DEBUG
   RARCH_DBG("[Shaders]: Number of passes: %u\n", shader->passes);
   RARCH_DBG("[Shaders]: Number of textures: %u\n", shader->luts);

   /* Log Texture Names & Paths */
   for (i = 0; i < shader->luts; i++)
      RARCH_DBG("[Shaders]: \"%s\" = \"%s\".\n", shader->lut[i].id,
            shader->lut[i].path);
#endif

   return true;
}

/**
 * override_shader_values:
 * @param override_conf
 * Config file who's values will be copied on top of conf
 * @param shader
 * Shader to be affected
 *
 * Takes values from override_config and overrides values of the shader
 *
 * @return 0 if nothing is overridden , 1 if something is overridden
 **/
static bool override_shader_values(config_file_t *override_conf,
      struct video_shader *shader)
{
   unsigned i;
   bool return_val                     = false;

   if (!shader || !override_conf) 
      return 0;

   /* If the shader has parameters */
   if (shader->num_parameters)
   {
      /* Step through the parameters in the shader and 
       * see if there is an entry for each in the override config */
      for (i = 0; i < shader->num_parameters; i++)
      {
         /* If the parameter is in the reference config */
         if (config_get_entry(override_conf, shader->parameters[i].id))
         {
            struct video_shader_parameter *parameter = 
               (struct video_shader_parameter*)
               video_shader_parse_find_parameter(
                     shader->parameters, 
                     shader->num_parameters, 
                     shader->parameters[i].id);

            /* Set the shader's parameter value */
            config_get_float(override_conf, shader->parameters[i].id,
                  &parameter->current);

#ifdef DEBUG
            RARCH_DBG("[Shaders]: Parameter: \"%s\" = %f.\n",
                  shader->parameters[i].id, 
                  shader->parameters[i].current);
#endif

            return_val = true;
         }
      }
   }

   /* ---------------------------------------------------------------------------------
    * ------------- Resolve Override texture paths to absolute paths-------------------
    * --------------------------------------------------------------------------------- */

   /* If the shader has textures */
   if (shader->luts)
   {
      char *override_tex_path             = (char*)malloc(PATH_MAX_LENGTH);

      override_tex_path[0]                = '\0';

      /* Step through the textures in the shader and see if there is an entry 
       * for each in the override config */
      for (i = 0; i < shader->luts; i++)
      {
         /* If the texture is defined in the reference config */
         if (config_get_entry(override_conf, shader->lut[i].id))
         {
            /* Texture path from shader the config */
            config_get_path(override_conf, shader->lut[i].id,
                  override_tex_path, PATH_MAX_LENGTH);

            /* Get the absolute path */
            fill_pathname_expanded_and_absolute(shader->lut[i].path,
                  override_conf->path, override_tex_path);

#ifdef DEBUG
            RARCH_DBG("[Shaders]: Texture: \"%s\" = %s.\n",
                        shader->lut[i].id, 
                        shader->lut[i].path);
#endif

            return_val = true;
         }
      }

      free(override_tex_path);
   }

   return return_val;
}

/**
 * video_shader_write_preset:
 * @param path
 * File to write to
 * @param shader
 * Shader to write
 * @param reference
 * Whether a simple preset should be written 
 * with the #reference to another preset in it.
 *
 * Writes a preset to disk. Can be written as a simple preset 
 * (With the #reference directive in it) or a full preset.
 * @return true on success, otherwise false on failure
 **/
bool video_shader_write_preset(const char *path,
      const char *shader_dir,
      const struct video_shader *shader, 
      bool reference)
{
   /* We need to clean up paths to be able to properly process them
    * path and shader->loaded_preset_path can use '/' on 
    * Windows due to Qt being Qt */
   char preset_dir[PATH_MAX_LENGTH];

   if (!shader || string_is_empty(path))
      return false;

   fill_pathname_join(preset_dir, shader_dir, "presets", sizeof(preset_dir));

   /* If we should still save a referenced preset do it now */
   if (reference)
   {
      if (video_shader_write_referenced_preset(path, shader_dir, shader))
         return true;

      RARCH_WARN("[Shaders]: Failed writing simple preset to \"%s\" "
            "- Full preset will be saved instead.\n", path);
   }

   /* If we aren't saving a referenced preset or weren't able to save one
    * then save a full preset */
   if (path)
      return video_shader_write_root_preset(shader, path);
   return false;
}

/**
 * video_shader_load_preset_into_shader:
 * @param path
 * Path to preset file, could be a 
 * Simple Preset (including a #reference) or Full Preset.
 * @param shader
 * Shader.
 *
 * Loads preset file to a shader including passes, textures 
 * and parameters
 *
 * @return true on success, otherwise false on failure.
 **/
bool video_shader_load_preset_into_shader(const char *path,
      struct video_shader *shader)
{
   bool ret                                          = true;
   config_file_t *conf                               = NULL;
   struct path_linked_list* override_paths_list      = NULL;
   struct path_linked_list* path_list_tmp            = NULL;
   config_file_t *root_conf                          = video_shader_get_root_preset_config(path);

   if (!root_conf)
   {
      RARCH_LOG("\n");
      RARCH_WARN("[Shaders]: Could not read root preset: \"%s\".\n", path);
      ret = false;
      goto end;
   }

   /* Check if the root preset is a valid shader chain */
   /* If the config has a shaders entry then it is considered 
      a shader chain config, vs a config which may only have 
      parameter values and texture overrides
   */
   if (!config_get_entry(root_conf, "shaders"))
   {
      RARCH_LOG("\n");
      RARCH_WARN("[Shaders]: Root preset is not a valid shader chain because it has no shaders entry: \"%s\".\n", path);
      ret = false;
      goto end;
   }

  /* If the root_conf path matches the original path then 
    * there are no references so we just load it and go to the end */
   if (string_is_equal(root_conf->path, path))
   {
      /* Load the config from the shader chain from the first reference into the shader */
      video_shader_load_root_config_into_shader(root_conf, config_get_ptr(), shader);
      goto end;
   }
   
   /* Get the config from the initial preset file 
    * We don't need to check it's validity because it must 
    * have been valid to get the root preset */
   conf = config_file_new_from_path_to_string(path);

#ifdef DEBUG
   RARCH_DBG("\n[Shaders]: Crawl preset reference chain..\n");
#endif

   /**
    * Check references starting with the second to make sure 
    * they do not have a shader chains in them
   **/
   path_list_tmp = (struct path_linked_list*)conf->references->next;
   while (path_list_tmp)
   {
      config_file_t *tmp_conf = NULL;
      char *path_to_ref       = (char*)malloc(PATH_MAX_LENGTH);

      fill_pathname_expanded_and_absolute(path_to_ref, conf->path, path_list_tmp->path);

      if ((tmp_conf = video_shader_get_root_preset_config(path_to_ref)))
      {
         /* Check if the config is a valid shader chain config */
         /* If the config has a shaders entry then it is considered 
            a shader chain config, vs a config which may only have 
            parameter values and texture overrides
          */
         if (config_get_entry(tmp_conf, "shaders"))
         {
            RARCH_WARN("\n[Shaders]: Additional #reference entries pointing at shader chain presets are not supported: \"%s\".\n", path_to_ref);
            config_file_free(tmp_conf);
            ret = false;
            goto end;
         }
         config_file_free(tmp_conf);
      }
      else
      {
         RARCH_WARN("\n[Shaders]: Could not load root preset for #reference entry: \"%s\".\n", path_to_ref);
         ret = false;
         goto end;
      }
      path_list_tmp = path_list_tmp->next;
   }

   /* Load the config from the shader chain from the first reference into the shader */
   video_shader_load_root_config_into_shader(root_conf, config_get_ptr(), shader);

   /* Set Path for originally loaded preset because it is different than the root preset path */
   strlcpy(shader->loaded_preset_path, path, sizeof(shader->loaded_preset_path));

#ifdef DEBUG
   RARCH_DBG("\n[Shaders]: Start applying simple preset overrides..\n");
#endif

   /* Gather all the paths of all of the presets in all reference chains */
   override_paths_list = path_linked_list_new();
   gather_reference_path_list(override_paths_list, conf->path, 0);

   /* 
    * Step through the references and apply overrides for each one
    * Start on the second item since the first is empty 
   */
   path_list_tmp = (struct path_linked_list*)override_paths_list;
   while (path_list_tmp)
   {
      config_file_t *override_conf = config_file_new_from_path_to_string(path_list_tmp->path);
#ifdef DEBUG
      RARCH_DBG("[Shaders]: Apply values from: \"%s\".\n", override_conf->path);
#endif
      override_shader_values(override_conf, shader);
      config_file_free(override_conf);
      path_list_tmp = path_list_tmp->next;
   }
   
#ifdef DEBUG
   RARCH_DBG("[Shaders]: End apply overrides.\n\n");
#endif

end:

   path_linked_list_free(override_paths_list);
   config_file_free(conf);
   config_file_free(root_conf);

   return ret;
}

const char *video_shader_type_to_str(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_CG:
         return "Cg";
      case RARCH_SHADER_HLSL:
         return "HLSL";
      case RARCH_SHADER_GLSL:
         return "GLSL";
      case RARCH_SHADER_SLANG:
         return "Slang";
      case RARCH_SHADER_METAL:
         return "Metal";
      case RARCH_SHADER_NONE:
         return "none";
      default:
         break;
   }

   return "???";
}

/**
 * video_shader_is_supported:
 * Tests if a shader type is supported.
 * This is only accurate once the context driver was initialized.

 * @return true on success, otherwise false on failure.
 **/
bool video_shader_is_supported(enum rarch_shader_type type)
{
   gfx_ctx_flags_t flags;
   enum display_flags testflag = GFX_CTX_FLAGS_NONE;

   flags.flags     = 0;

   switch (type)
   {
      case RARCH_SHADER_SLANG:
         testflag = GFX_CTX_FLAGS_SHADERS_SLANG;
         break;
      case RARCH_SHADER_GLSL:
         testflag = GFX_CTX_FLAGS_SHADERS_GLSL;
         break;
      case RARCH_SHADER_CG:
         testflag = GFX_CTX_FLAGS_SHADERS_CG;
         break;
      case RARCH_SHADER_HLSL:
         testflag = GFX_CTX_FLAGS_SHADERS_HLSL;
         break;
      case RARCH_SHADER_NONE:
      default:
         return false;
   }
   video_context_driver_get_flags(&flags);

   return BIT32_GET(flags.flags, testflag);
}

const char *video_shader_get_preset_extension(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_GLSL:
         return ".glslp";
      case RARCH_SHADER_SLANG:
         return ".slangp";
      case RARCH_SHADER_HLSL:
      case RARCH_SHADER_CG:
         return ".cgp";
      default:
         break;
   }

   return NULL;
}

bool video_shader_any_supported(void)
{
   gfx_ctx_flags_t flags;
   flags.flags     = 0;
   video_context_driver_get_flags(&flags);

   return
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_SLANG) ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_GLSL)  ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_CG)    ||
      BIT32_GET(flags.flags, GFX_CTX_FLAGS_SHADERS_HLSL);
}

enum rarch_shader_type video_shader_get_type_from_ext(
      const char *ext, bool *is_preset)
{
   if (string_is_empty(ext))
      return RARCH_SHADER_NONE;

   if ((ext[0] != '\0') && (ext[0] == '.') && (ext[1] != '\0'))
      ext++;

   if (is_preset)
      *is_preset =
         string_is_equal_case_insensitive(ext, "cgp")   ||
         string_is_equal_case_insensitive(ext, "glslp") ||
         string_is_equal_case_insensitive(ext, "slangp");

   if (string_is_equal_case_insensitive(ext, "cgp") ||
       string_is_equal_case_insensitive(ext, "cg")
      )
      return RARCH_SHADER_CG;

   if (string_is_equal_case_insensitive(ext, "glslp") ||
       string_is_equal_case_insensitive(ext, "glsl")
      )
      return RARCH_SHADER_GLSL;

   if (string_is_equal_case_insensitive(ext, "slangp") ||
       string_is_equal_case_insensitive(ext, "slang")
      )
      return RARCH_SHADER_SLANG;

   return RARCH_SHADER_NONE;
}

bool video_shader_check_for_changes(void)
{
   if (!file_change_data)
      return false;

   return frontend_driver_check_for_path_changes(file_change_data);
}

void dir_free_shader(
      struct rarch_dir_shader_list *dir_list,
      bool shader_remember_last_dir)
{
   if (dir_list->shader_list)
   {
      dir_list_free(dir_list->shader_list);
      dir_list->shader_list = NULL;
   }

   if (dir_list->directory)
   {
      free(dir_list->directory);
      dir_list->directory = NULL;
   }

   dir_list->selection                = 0;
   dir_list->shader_loaded            = false;
   dir_list->remember_last_preset_dir = shader_remember_last_dir;
}

static bool dir_init_shader_internal(
      bool shader_remember_last_dir,
      struct rarch_dir_shader_list *dir_list,
      const char *shader_dir,
      const char *shader_file_name,
      bool show_hidden_files)
{
   size_t i;
   struct string_list *new_list           = dir_list_new_special(
         shader_dir, DIR_LIST_SHADERS, NULL, show_hidden_files);
   bool search_file_name                  = shader_remember_last_dir &&
         !string_is_empty(shader_file_name);

   if (!new_list)
      return false;

   if (new_list->size < 1)
   {
      dir_list_free(new_list);
      return false;
   }

   dir_list_sort(new_list, false);

   dir_list->shader_list              = new_list;
   dir_list->directory                = strdup(shader_dir);
   dir_list->selection                = 0;
   dir_list->shader_loaded            = false;
   dir_list->remember_last_preset_dir = shader_remember_last_dir;

   if (search_file_name)
   {
      for (i = 0; i < new_list->size; i++)
      {
         const char *file_name = NULL;
         const char *file_path = new_list->elems[i].data;

         if (string_is_empty(file_path))
            continue;

         /* If a shader file name has been provided,
          * search the list for a match and set 'selection'
          * index if found */
         file_name = path_basename(file_path);

         if ( !string_is_empty(file_name) &&
               string_is_equal(file_name, shader_file_name))
         {
            RARCH_LOG("[Shaders]: %s \"%s\".\n",
                  msg_hash_to_str(MSG_FOUND_SHADER),
                  file_path);

            dir_list->selection = i;
            break;
         }
      }
   }

   return true;
}

void dir_init_shader(
      void *menu_driver_data_,
      settings_t *settings,
      struct rarch_dir_shader_list *dir_list)
{
   bool show_hidden_files                         = settings->bools.show_hidden_files;
   bool shader_remember_last_dir                  = settings->bools.video_shader_remember_last_dir;
   const char *directory_video_shader             = settings->paths.directory_video_shader;
   const char *directory_menu_config              = settings->paths.directory_menu_config;
   bool video_shader_remember_last_dir            = settings->bools.video_shader_remember_last_dir;
   const char *last_shader_preset_dir             = NULL;
   const char *last_shader_preset_file_name       = NULL;
#if defined(HAVE_MENU)
   menu_handle_t *menu                            = (menu_handle_t*)menu_driver_data_;
   enum rarch_shader_type last_shader_preset_type = menu ? menu->last_shader_selection.preset_type : RARCH_SHADER_NONE;
   menu_driver_get_last_shader_preset_path(
         &last_shader_preset_dir, &last_shader_preset_file_name);
#else
   enum rarch_shader_type last_shader_preset_type = RARCH_SHADER_NONE;
#endif

   /* Always free existing shader list */
   dir_free_shader(dir_list,
         video_shader_remember_last_dir);

   /* Try directory of last selected shader preset */
   if (shader_remember_last_dir &&
       (last_shader_preset_type != RARCH_SHADER_NONE) &&
       !string_is_empty(last_shader_preset_dir) &&
       dir_init_shader_internal(
          video_shader_remember_last_dir,
          dir_list,
          last_shader_preset_dir,
          last_shader_preset_file_name,
          show_hidden_files))
      return;

   /* Try video shaders directory */
   if (!string_is_empty(directory_video_shader) &&
       dir_init_shader_internal(
            video_shader_remember_last_dir,
            dir_list,
            directory_video_shader, NULL, show_hidden_files))
      return;

   /* Try config directory */
   if (!string_is_empty(directory_menu_config) &&
       dir_init_shader_internal(
            video_shader_remember_last_dir,
            dir_list,
            directory_menu_config, NULL, show_hidden_files))
      return;

   /* Try 'top level' directory containing main
    * RetroArch config file */
   if (!path_is_empty(RARCH_PATH_CONFIG))
   {
      char *rarch_config_directory = strdup(path_get(RARCH_PATH_CONFIG));
      path_basedir(rarch_config_directory);

      if (!string_is_empty(rarch_config_directory))
         dir_init_shader_internal(
               video_shader_remember_last_dir,
               dir_list,
               rarch_config_directory, NULL, show_hidden_files);

      free(rarch_config_directory);
   }
}

void dir_check_shader(
      void *menu_driver_data_,
      settings_t *settings,
      struct rarch_dir_shader_list *dir_list,
      bool pressed_next,
      bool pressed_prev)
{
   bool video_shader_remember_last_dir            = settings->bools.video_shader_remember_last_dir;
   const char *last_shader_preset_dir             = NULL;
   const char *last_shader_preset_file_name       = NULL;
   const char *set_shader_path                    = NULL;
   bool dir_list_initialised                      = false;
#if defined(HAVE_MENU)
   void *menu_ptr                                 = menu_driver_data_;
   menu_handle_t *menu                            = (menu_handle_t*)menu_ptr;
   enum rarch_shader_type last_shader_preset_type = menu ? menu->last_shader_selection.preset_type : RARCH_SHADER_NONE;
   menu_driver_get_last_shader_preset_path(
         &last_shader_preset_dir, &last_shader_preset_file_name);
#else
   void *menu_ptr                                 = NULL;
   enum rarch_shader_type last_shader_preset_type = RARCH_SHADER_NONE;
#endif

   /* Check whether shader list needs to be
    * (re)initialised */
   if (!dir_list->shader_list ||
       (dir_list->remember_last_preset_dir != video_shader_remember_last_dir) ||
       (video_shader_remember_last_dir &&
        (last_shader_preset_type != RARCH_SHADER_NONE) &&
        !string_is_equal(dir_list->directory, last_shader_preset_dir)))
   {
      dir_init_shader(menu_ptr, settings, dir_list);
      dir_list_initialised = true;
   }

   if (!dir_list->shader_list ||
       (dir_list->shader_list->size < 1))
      return;

   /* Check whether a 'last used' shader file
    * name is provided
    * > Note: We can end up calling
    *   string_is_equal(dir_list->directory, last_shader_preset_dir)
    *   twice. This is wasteful, but we cannot safely cache
    *   the first result since dir_init_shader() is called
    *   in-between the two invocations... */
   if (video_shader_remember_last_dir &&
       (last_shader_preset_type != RARCH_SHADER_NONE) &&
       string_is_equal(dir_list->directory, last_shader_preset_dir) &&
       !string_is_empty(last_shader_preset_file_name))
   {
      /* Ensure that we start with a dir_list selection
       * index matching the last used shader */
      if (!dir_list_initialised)
      {
         const char *current_file_path = NULL;
         const char *current_file_name = NULL;

         if (dir_list->selection < dir_list->shader_list->size)
            current_file_path = dir_list->shader_list->elems[dir_list->selection].data;

         if (!string_is_empty(current_file_path))
            current_file_name = path_basename(current_file_path);

         if (!string_is_empty(current_file_name) &&
             !string_is_equal(current_file_name, last_shader_preset_file_name))
         {
            size_t i;
            for (i = 0; i < dir_list->shader_list->size; i++)
            {
               const char *file_path = dir_list->shader_list->elems[i].data;
               const char *file_name = NULL;

               if (string_is_empty(file_path))
                  continue;

               file_name = path_basename(file_path);

               if (string_is_empty(file_name))
                  continue;

               if (string_is_equal(file_name, last_shader_preset_file_name))
               {
                  dir_list->selection = i;
                  break;
               }
            }
         }
      }

#ifdef HAVE_MENU
      /* Check whether the shader referenced by the
       * current selection index is already loaded */
      if (!dir_list->shader_loaded)
      {
         struct video_shader *shader = menu_shader_get();

         if (shader && !string_is_empty(shader->loaded_preset_path))
         {
            char last_shader_path[PATH_MAX_LENGTH];
            fill_pathname_join(last_shader_path,
                  last_shader_preset_dir, last_shader_preset_file_name,
                  sizeof(last_shader_path));

            if (string_is_equal(last_shader_path, shader->loaded_preset_path))
               dir_list->shader_loaded = true;
         }
      }
#endif
   }

   /* Select next shader in list */
   if (pressed_next)
   {
      /* Only increment selection if a shader
       * from this list has already been loaded
       * (otherwise first entry in the list may
       * be skipped) */
      if (dir_list->shader_loaded)
      {
         if (dir_list->selection < dir_list->shader_list->size - 1)
            dir_list->selection++;
         else
            dir_list->selection = 0;
      }
   }
   /* Select previous shader in list */
   else if (pressed_prev)
   {
      if (dir_list->selection > 0)
         dir_list->selection--;
      else
         dir_list->selection = dir_list->shader_list->size - 1;
   }
   else
      return;

   set_shader_path = dir_list->shader_list->elems[dir_list->selection].data;
#if defined(HAVE_MENU)
   menu_driver_set_last_shader_preset_path(set_shader_path);
#endif
   command_set_shader(NULL, set_shader_path);
   dir_list->shader_loaded = true;
}

static bool retroarch_load_shader_preset_internal(
      char *s,
      size_t len,
      const char *shader_directory,
      const char *core_name,
      const char *special_name)
{
   unsigned i;

   static enum rarch_shader_type types[] =
   {
      /* Shader preset priority, highest to lowest
       * only important for video drivers with multiple shader backends */
      RARCH_SHADER_GLSL, RARCH_SHADER_SLANG, RARCH_SHADER_CG, RARCH_SHADER_HLSL
   };

   for (i = 0; i < ARRAY_SIZE(types); i++)
   {
      if (!video_shader_is_supported(types[i]))
         continue;

      /* Concatenate strings into full paths */
      if (!string_is_empty(core_name))
         fill_pathname_join_special_ext(s,
               shader_directory, core_name,
               special_name,
               video_shader_get_preset_extension(types[i]),
               len);
      else
      {
         if (string_is_empty(special_name))
            break;

         fill_pathname_join(s, shader_directory, special_name, len);
         strlcat(s, video_shader_get_preset_extension(types[i]), len);
      }

      if (path_is_valid(s))
         return true;
   }

   return false;
}

bool load_shader_preset(settings_t *settings, const char *core_name,
      char *s, size_t len)
{
   const char *video_shader_directory = settings->paths.directory_video_shader;
   const char *menu_config_directory  = settings->paths.directory_menu_config;
   const char *rarch_path_basename    = path_get(RARCH_PATH_BASENAME);
   bool has_content                   = !string_is_empty(rarch_path_basename);

   const char *game_name              = NULL;
   const char *dirs[3]                = {0};
   size_t i                           = 0;

   char shader_path[PATH_MAX_LENGTH];
   char content_dir_name[PATH_MAX_LENGTH];
   char config_file_directory[PATH_MAX_LENGTH];
   char old_presets_directory[PATH_MAX_LENGTH];

   shader_path[0]                     = '\0';
   content_dir_name[0]                = '\0';
   config_file_directory[0]           = '\0';
   old_presets_directory[0]           = '\0';

   if (has_content)
   {
      fill_pathname_parent_dir_name(content_dir_name,
            rarch_path_basename, sizeof(content_dir_name));
      game_name = path_basename(rarch_path_basename);
   }

   if (!path_is_empty(RARCH_PATH_CONFIG))
      fill_pathname_basedir(config_file_directory,
            path_get(RARCH_PATH_CONFIG), sizeof(config_file_directory));

   if (!string_is_empty(video_shader_directory))
      fill_pathname_join(old_presets_directory,
         video_shader_directory, "presets", sizeof(old_presets_directory));

   dirs[0]                            = menu_config_directory;
   dirs[1]                            = config_file_directory;
   dirs[2]                            = old_presets_directory;

   for (i = 0; i < ARRAY_SIZE(dirs); i++)
   {
      if (string_is_empty(dirs[i]))
         continue;
      /* Game-specific shader preset found? */
      if (has_content && retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               game_name))
         goto success;
      /* Folder-specific shader preset found? */
      if (has_content && retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               content_dir_name))
         goto success;
      /* Core-specific shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], core_name,
               core_name))
         goto success;
      /* Global shader preset found? */
      if (retroarch_load_shader_preset_internal(
               shader_path,
               sizeof(shader_path),
               dirs[i], NULL,
               "global"))
         goto success;
   }
   return false;

success:
   /* Shader preset exists, load it. */
   strlcpy(s, shader_path, len);
   return true;
}

bool apply_shader(
      settings_t *settings,
      enum rarch_shader_type type,
      const char *preset_path, bool message)
{
   char msg[256];
   video_driver_state_t 
      *video_st                 = video_state_get_ptr();
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   const char      *core_name   = runloop_st->system.info.library_name;
   const char      *preset_file = NULL;
#ifdef HAVE_MENU
   struct video_shader *shader  = menu_shader_get();
#endif

   /* Disallow loading shaders when no core is loaded */
   if (string_is_empty(core_name))
      return false;

   if (!string_is_empty(preset_path))
      preset_file = path_basename_nocompression(preset_path);

   /* TODO/FIXME - This loads the shader into the video driver
    * But then we load the shader from disk twice more to put it in the menu
    * We need to reconfigure this at some point to only load it once */
   if (video_st->current_video->set_shader)
   {
      if ((video_st->current_video->set_shader(
                  video_st->data, type, preset_path)))
      {
         configuration_set_bool(settings, settings->bools.video_shader_enable, true);
         if (!string_is_empty(preset_path))
         {
            strlcpy(runloop_st->runtime_shader_preset_path, preset_path,
                  sizeof(runloop_st->runtime_shader_preset_path));
#ifdef HAVE_MENU
            /* reflect in shader manager */
            if (menu_shader_manager_set_preset(
                     shader, type, preset_path, false))
               shader->modified = false;
#endif
         }
         else
            runloop_st->runtime_shader_preset_path[0] = '\0';

         if (message)
         {
            /* Display message */
            const char *msg_shader = msg_hash_to_str(MSG_SHADER);
            size_t _len            = strlcpy(msg, msg_shader, sizeof(msg));
            msg[_len  ]            = ':';
            msg[_len+1]            = ' ';
            msg[_len+2]            = '\0';
            if (preset_file)
            {
               msg[_len+2]         = '"';
               msg[_len+3]         = '\0';
               _len                = strlcat(msg, preset_file, sizeof(msg));
               msg[_len  ]        = '"';
               msg[_len+1]        = '\0';
            }
            else
               strlcat(msg, msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NONE), sizeof(msg));

#ifdef HAVE_GFX_WIDGETS
            if (dispwidget_get_ptr()->active)
               gfx_widget_set_generic_message(msg, 2000);
            else
#endif
               runloop_msg_queue_push(msg, 1, 120, true, NULL,
                     MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         }

         RARCH_LOG("[Shaders]: %s: \"%s\".\n",
               msg_hash_to_str(MSG_APPLYING_SHADER),
               preset_path ? preset_path : "null");

         return true;
      }
   }

#ifdef HAVE_MENU
   /* reflect in shader manager */
   menu_shader_manager_set_preset(shader, type, NULL, false);
#endif

   /* Display error message */
   fill_pathname_join_delim(msg,
         msg_hash_to_str(MSG_FAILED_TO_APPLY_SHADER_PRESET),
         preset_file ? preset_file : "null",
         ' ',
         sizeof(msg));

   runloop_msg_queue_push(
         msg, 1, 180, true, NULL,
         MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_ERROR);
   return false;
}

/* get the name of the current shader preset */
const char *retroarch_get_shader_preset(void)
{
settings_t *settings           = config_get_ptr();
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   const char *core_name          = runloop_st->system.info.library_name;
   bool video_shader_enable       = settings->bools.video_shader_enable;
   unsigned video_shader_delay    = settings->uints.video_shader_delay;
   bool auto_shaders_enable       = settings->bools.auto_shaders_enable;
   bool cli_shader_disable        = video_st->cli_shader_disable;
   
   read_cfg_settting();
   char conf_path[PATH_MAX_LENGTH];
   char app_path[PATH_MAX_LENGTH]         = {0};
#if WIN32
      fill_pathname_home_dir(app_path, sizeof(app_path));
#else
      fill_pathname_application_dir(app_path, sizeof(app_path));
      //printf(app_path);
#endif
  /* if (!bIsKey)
   {
     settings->uints.video_shader_type=0;
   }*/

   printf("\nKey_33333333333\n");

   char ch[256];
   sprintf(ch,"video_shader_type:%d----video_shader_level:%d\n",settings->uints.video_shader_type,settings->uints.video_shader_level);
   printf(ch);  

   if (settings->uints.video_shader_type==1)
   {
      if(settings->uints.video_shader_level==1)
      {
         //printf("1111");
         if(settings->paths.video_shader_path1==NULL)
         {
            return NULL;
         }
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path1, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==2)
      {
         //printf("2222");
         if(settings->paths.video_shader_path2==NULL)
         {
            return NULL;
         }
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path2, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==3)
      {
         if(settings->paths.video_shader_path3==NULL)
         {
            return NULL;
         }
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path3, sizeof(conf_path));
      }
      else if(settings->uints.video_shader_level==4)
      {
         if(settings->paths.video_shader_path4==NULL)
         {
            return NULL;
         }
         fill_pathname_resolve_relative(conf_path, app_path,
         settings->paths.video_shader_path4, sizeof(conf_path));
      }

      strlcpy(runloop_st->runtime_shader_preset_path,
         conf_path,sizeof(runloop_st->runtime_shader_preset_path));

      printf("滤镜2:%s\n", runloop_st->runtime_shader_preset_path);
      printf("\n");
      return runloop_st->runtime_shader_preset_path;
   }
#if OLDCPP
   if (!video_shader_enable)
      return NULL;

   if (video_shader_delay && !runloop_st->shader_delay_timer.timer_end)
      return NULL;

   /* Disallow loading auto-shaders when no core is loaded */
  if (string_is_empty(core_name))
      return NULL;

   if (!string_is_empty(runloop_st->runtime_shader_preset_path))
      return runloop_st->runtime_shader_preset_path;

   /* load auto-shader once, --set-shader works like a global auto-shader */
   if (video_st->shader_presets_need_reload && !cli_shader_disable)
   {
      video_st->shader_presets_need_reload = false;

      if (video_shader_is_supported(
               video_shader_parse_type(video_st->cli_shader_path)))
         strlcpy(runloop_st->runtime_shader_preset_path,
               video_st->cli_shader_path,
               sizeof(runloop_st->runtime_shader_preset_path));
      else
      {
         if (auto_shaders_enable) /* sets runtime_shader_preset_path */
         {
            if (load_shader_preset(
                     settings,
                     runloop_st->system.info.library_name,
                     runloop_st->runtime_shader_preset_path,
                     sizeof(runloop_st->runtime_shader_preset_path)))
            {
               RARCH_LOG("[Shaders]: Specific shader preset found at \"%s\".\n",
                     runloop_st->runtime_shader_preset_path);
            }
         }
      }
      
      return runloop_st->runtime_shader_preset_path;
   }
#endif
   return NULL;
}
