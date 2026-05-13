```This project has been created as part of the 42 curriculum by mosokina, aistok, and aaladeok```

# WebServ (PDF v24.0)

## Description

...

## Instructions

Run the 42 tester:

1. make (to compile the webserv binary in ./bin/webserv)
2. run:
   
   ./bin/webserv config/42tester.conf (to start the webserv)
   
3. from a separate terminal, run:
   
   ./tests/42tester http://localhost:8080

   And follow the instructions (just hit ENTER) as everything
   is configured as asked (www_42tester has the needed files
   and directories, and conf/42tester.conf has the needed
   configuration)

Test from web browser:

1. after steps (1) and (2) from above, go to the URL in FireFox:

   http://localhost:8080

   The page should load, allowing some manual testing.

2. for manual file uploads, there are a few files in

   ./tests/files_to_upload

   To make it easier.

3. in order to be able to test the DELETE requests, the folder

   www/delete_test

   has some files and folders READ ONLY (ex. chmod 444 file,
   or chmod 555 file, etc); to clean these up easily, run

   make fclean

   which will call a shell script that will change the
   access rights and deletes the www/delete_test if needed.

## Resources

- HTTP 1.0 RFC:
  https://datatracker.ietf.org/doc/html/rfc1945

- The Common Gateway Interface (CGI) Version 1.1 RFC:
  https://www.rfc-editor.org/rfc/rfc3875

- How the web works: HTTP and CGI explained
  https://www.garshol.priv.no/download/text/http-tut.html

- ...
