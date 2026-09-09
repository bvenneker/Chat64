
How to install your own chat server on a local test server.
If you are builing a live production server, YOU are responsible to make it secure.


1) Install Apache, PHP and MySQL. There is plenty of information on the internet on how to do this.

2) Unzip the server.zip into apaches default location (/var/www/html/)
   if you now go to localhost in a webbrowser, you should see the chat64 web site. But you will get an error
   when you go to 'See who is online', proceed to step 3.
   If you do not see the site at all (but maybe an error), fix that first.. chatgpt is your friend ;-)
   
3) create a database user with rights to create a new database.

    sudo mariadb
	CREATE USER admin@localhost IDENTIFIED BY 'yourStrongPassword';
	GRANT ALL PRIVILEGES ON *.* TO admin@localhost WITH GRANT OPTION;
	FLUSH PRIVILEGES;
	
4) goto localhost/install.php
   In this page, type your database admin user and password. The page will install the database.
   It also tries to create a dbCredent.php (one level up from /html/) file to store the database credentials, If that step fails, you
   are asked to create that file manually!

   here is an example of the files content:
   <?php
   $hostname = '127.0.0.1';
   $dbname   = 'chat64';
   $username = 'chat64_user';
   $password = 'ye637edded3383%re!jide8dd3jncjk398827hd';
   $conn = new mysqli($hostname, $username, $password, $dbname);
   ?>
   