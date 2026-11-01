@echo off

rem Set the service to auto-start, unless the user already chose another start type
sc qc ApolloService | findstr /C:"DEMAND_START" >nul && sc config ApolloService start= auto
