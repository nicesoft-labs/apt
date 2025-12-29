// -*- mode: c++; mode: fold -*-
// Описание                                                            /*{{{*/
// $Id: apt-config.cc,v 1.5 2003/01/29 18:43:48 niemeyer Exp $
/* ######################################################################

   APT Config - Program to manipulate APT configuration files

   This program will parse a config file and then do something with it.

   Commands:
     shell - Shell mode. After this a series of word pairs should occure.
             The first is the environment var to set and the second is
             the key to set it from. Use like:
 eval `apt-config shell QMode apt::QMode`

   ##################################################################### */
/*}}}*/
// Подключаемые файлы                                                   /*{{{*/
#include <apt-pkg/cmndline.h>
#include <apt-pkg/error.h>
#include <apt-pkg/init.h>
#include <apt-pkg/strutl.h>

#include <config.h>
#include <apti18n.h>

// CNC:2003-02-14 - apti18n.h includes libintl.h which includes locale.h,
//                  as reported by Radu Greab.
// В C++ корректнее подключить <clocale>, чтобы setlocale был объявлен явно.
#include <clocale>

#include <cstring>   // strlen
#include <iostream>
#include <string>
/*}}}*/

using namespace std;

// DoShell - обработка команды shell                                     /*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoShell(CommandLine &CmdL)
{
   for (const char **I = CmdL.FileList + 1; *I != 0; I += 2)
   {
      // Аргументы должны идти парами: <ENV_VAR> <config-key>
      // (Строку ошибки оставляем английской: msgid должен быть стабильным)
      if (I[1] == 0 || std::strlen(I[1]) == 0)
         return _error->Error(_("Arguments not in pairs"));

      string key = I[1];

      // Старый формат директорий: ключ заканчивается на '/'.
      // Историческое поведение: добавляем 'd'.
      if (!key.empty() && key[key.size() - 1] == '/')
         key.append("d");

      if (_config->ExistsAny(key.c_str()))
      {
         cout << *I << "='"
              << SubstVar(_config->FindAny(key.c_str()), "'", "'\\''")
              << '\'' << endl;
      }
   }

   return true;
}
/*}}}*/

// DoDump - вывести пространство конфигурации                             /*{{{*/
// ---------------------------------------------------------------------
/* */
bool DoDump(CommandLine &CmdL)
{
   (void)CmdL; // аргумент не используется, но сигнатуру не меняем
   _config->Dump(cout);
   return true;
}
/*}}}*/

// ShowHelp - показать справку                                           /*{{{*/
// ---------------------------------------------------------------------
/* */
int ShowHelp()
{
   ioprintf(cout,_("%s %s for %s %s compiled on %s %s\n"),PACKAGE,VERSION,
            COMMON_OS,COMMON_CPU,__DATE__,__TIME__);
   if (_config->FindB("version") == true)
      return 0;

   cout <<
    _("Usage: apt-config [options] command\n"
      "\n"
      "apt-config is a simple tool to read the APT config file\n"
      "\n"
      "Commands:\n"
      "   shell - Shell mode\n"
      "   dump - Show the configuration\n"
      "\n"
      "Options:\n"
      "  -h   This help text.\n"
      "  -c=? Read this configuration file\n"
      "  -o=? Set an arbitary configuration option, eg -o dir::cache=/tmp\n"
      "NiceSOFT LLC: ООО \"НАЙС СОФТ ГРУПП\" 5024245440 <niceos@ncsgp.ru>\n");
   return 0;
}
/*}}}*/

int main(int argc,const char *argv[])
{
   CommandLine::Args Args[] = {
      {'h',"help","help",0},
      {'v',"version","version",0},
      {'c',"config-file",0,CommandLine::ConfigFile},
      {'o',"option",0,CommandLine::ArbItem},
      {0,0,0,0}};
   CommandLine::Dispatch Cmds[] = {{"shell",&DoShell},
                                   {"dump",&DoDump},
                                   {0,0}};

   // Включаем gettext (переводы берутся из *.mo по textdomain(PACKAGE))
   setlocale(LC_ALL,"");
   textdomain(PACKAGE);

   // Разбор аргументов и инициализация подсистем APT
   CommandLine CmdL(Args,_config);
   if (pkgInitConfig(*_config) == false ||
       CmdL.Parse(argc,argv) == false ||
       pkgInitSystem(*_config,_system) == false)
   {
      _error->DumpErrors();
      return 100;
   }

   // Показать справку, если попросили или команда не указана
   if (_config->FindB("help") == true ||
       CmdL.FileSize() == 0)
      return ShowHelp();

   // Выполнить команду
   CmdL.DispatchArg(Cmds);

   // Печать ошибок/предупреждений
   if (_error->empty() == false)
   {
      bool Errors = _error->PendingError();
      _error->DumpErrors();
      return Errors == true?100:0;
   }

   return 0;
}
