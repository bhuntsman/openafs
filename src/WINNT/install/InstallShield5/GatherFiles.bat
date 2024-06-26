@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem
rem This file copies the IS5 files from the IS5 dirs to the CMl dir.


copy "Component Definitions\Default.cdf" .
copy "Component Definitions\Default.fgl" .

copy "File Groups\Default.fdf" .
copy "File Groups\GenFileGroups.bat" .

copy "Media\Transarc AFS\GenDefault.mda.bat" .

copy "Registry Entries\Default.rge" .

copy "Script Files\setup.rul" .

copy "Setup Files\Uncompressed Files\Language Independent\OS Independent\setup.bmp" .

copy "Shell Objects\Default.shl" .\Default.shell

copy "String Tables\Default.shl" .

copy "String Tables\0009-English\value.shl" lang\en_US
copy "String Tables\0011-Japanese\value.shl" lang\ja_JP
copy "String Tables\0012-Korean\value.shl" lang\ko_KR
copy "String Tables\0404-Chinese (Taiwan)\value.shl" lang\zh_TW
copy "String Tables\0804-Chinese (PRC)\value.shl" lang\zh_CN
copy "String Tables\0007-German\value.shl" lang\de_DE
copy "String Tables\0416-Portuguese (Brazilian)\value.shl" lang\pt_BR
copy "String Tables\000a-Spanish\value.shl" lang\es_ES

copy "Text Substitutions\Build.tsb" .
copy "Text Substitutions\Setup.tsb" .

