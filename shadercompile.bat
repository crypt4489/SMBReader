@ECHO off
@SETLOCAL EnableDelayedExpansion
CD ./shaders/
FOR %%s in (*) DO CALL :CheckFile %%s 
GOTO EndOfProgram

:CheckFile
SET OutputFile=./compiled/%1.spv
IF EXIST %OutputFile% (
	FOR %%f IN (%OutputFile%) DO SET OutputFileTime=%%~tf
	FOR %%f IN (%1) DO SET InputFileTime=%%~tf

	SET InputYear=!InputFileTime:~6,4!
	SET OutputYear=!OutputFileTime:~6,4!

	SET /A InputYear=!InputYear!
	SET /A OutputYear=!OutputYear!

	IF !InputYear! LEQ !OutputYear! (

		IF !InputYear! LSS !OutputYear! (
			goto :eof
		)

		SET InputMonth=!InputFileTime:~0,2!
		SET OutputMonth=!OutputFileTime:~0,2!

		IF "!InputMonth:~0,1!" == "0" SET InputMonth=!InputMonth:~1,1!
		IF "!OutputMonth:~0,1!" == "0" SET OutputMonth=!OutputMonth:~1,1!

		SET /A InputMonth=!InputMonth!
		SET /A OutputMonth=!OutputMonth!

		IF !InputMonth! LEQ !OutputMonth! (

			IF !InputMonth! LSS !OutputMonth! (
				goto :eof
			)

			SET InputDay=!InputFileTime:~3,2!
			SET OutputDay=!OutputFileTime:~3,2!

			IF "!InputDay:~0,1!" == "0" SET InputDay=!InputDay:~1,1!
			IF "!OutputDay:~0,1!" == "0" SET OutputDay=!OutputDay:~1,1!

			SET /A InputDay=!InputDay!
			SET /A OutputDay=!OutputDay!

			IF !InputDay! LEQ !OutputDay! (


				IF !InputDay! LSS !OutputDay! (
					goto :eof
				)
				

				SET InputHour=!InputFileTime:~11,2!
				SET OutputHour=!OutputFileTime:~11,2!

				IF "!InputHour:~0,1!"=="0" SET InputHour=!InputHour:~1,1!
				IF "!OutputHour:~0,1!"=="0" SET OutputHour=!OutputHour:~1,1!

				SET twelvhourdivideIn=!InputFileTime:~-2! 
				SET twelvhourdivideOut=!OutputFileTime:~-2! 

				SET /A inoff=0
				SET /A outoff=0
	
				IF "!twelvhourdivideIn:~0,2!"=="PM" (
					IF !InputHour! NEQ 12 (
						SET /A inoff=12
					)
				)

				IF "!twelvhourdivideOut:~0,2!"=="PM" (
					IF !OutputHour! NEQ 12 (
						SET /A outoff=12
					)
				)

				SET /A InputHour=!InputHour!+!inOfF!
				SET /A OutputHour=!OutputHour!+!outoff!

				IF !InputHour! LEQ !OutputHour! (

					IF !InputHour! LSS !OutputHour! ( 
						goto :eof
					)

					SET InputMinutes=!InputFileTime:~14,2!
					SET OutputMinutes=!OutputFileTime:~14,2!

					IF "!InputMinutes:~0,1!"=="0" SET InputMinutes=!InputMinutes:~1,1!
					IF "!OutputMinutes:~0,1!"=="0" SET OutputMinutes=!OutputMinutes:~1,1!

					
					SET /A InputMinutes=!InputMinutes!
					SET /A OutputMinutes=!OutputMinutes!

					IF !InputMinutes! LEQ !OutputMinutes! (
						goto :eof
					)
				)
			)
		)
	)
) 

:WRITEOUT
glslc.exe -g -O0 --target-env=vulkan1.3 --target-spv=spv1.6 %1 -o %OutputFile%

GOTO :eof

:EndOfProgram
@ENDLOCAL