@echo off

rem
rem Copyright (C) 1998  Transarc Corporation.
rem All rights reserved.
rem
rem

rem -------------- default.mda --------------------------------------------

echo [Platforms] > default.mda
echo key0=0000000000010000 >> default.mda
echo count=1 >> default.mda
echo. >> default.mda
echo [Filter] >> default.mda
echo LANGUAGEDEFAULT=0009 >> default.mda
echo. >> default.mda
echo [GeneralInfo] >> default.mda
echo MEDIATYPE=CDROM >> default.mda
echo DATAASFILES=No >> default.mda
echo BUILDTYPE=Full >> default.mda
echo BuildLocation=Media\Transarc AFS >> default.mda
echo BREAKBYTOPCOMPONENT=No >> default.mda
echo REFRESH_DATAFILES=Yes >> default.mda
echo REFRESH_SPLASH=Yes >> default.mda
echo BUILDSIZE= >> default.mda
echo Name=AFS for Windows >> default.mda
echo REVIEWREPORT=No >> default.mda
echo REFRESH_IFILES=Yes >> default.mda
echo PASSWORD= >> default.mda
echo. >> default.mda
echo [SetupInfo] >> default.mda
echo ENABLELANGDLG=Yes >> default.mda
echo ALTERNATEISDELETENAME= >> default.mda
echo APPLICATIONNAME=AFS for Windows >> default.mda
echo HIDESTATUSBAR=No >> default.mda
echo WIN32SENABLE=No >> default.mda
echo. >> default.mda
echo [InstallDateInfo] >> default.mda
echo TYPE=BUILDTIME >> default.mda
echo TIME= >> default.mda
echo DATE= >> default.mda
echo. >> default.mda
echo [FileInstallDateInfo] >> default.mda
echo TYPE=ORIGINAL >> default.mda
echo TIME= >> default.mda
echo DATE= >> default.mda
echo. >> default.mda
echo [Languages] >> default.mda
echo key0=0009 >> default.mda
echo key1=0416 >> default.mda
echo key2=0804 >> default.mda
echo key3=0404 >> default.mda
echo key4=0007 >> default.mda
echo key5=0011 >> default.mda
echo key6=0012 >> default.mda
echo key7=000a >> default.mda
echo count=8 >> default.mda
echo. >> default.mda
echo [TagFileInfo] >> default.mda
echo PRODUCTCATEGORY=System Tool >> default.mda
echo APPLICATIONNAME=AFS for Windows >> default.mda
echo COMPANYNAME=Transarc Corpration >> default.mda
echo VERSION=1.00.000 >> default.mda
echo INFO= >> default.mda

