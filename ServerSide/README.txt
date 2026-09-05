
How to install your own chat server on a local test server.

1) Install Apache, PHP and MySQL. There is plenty of information on the internet on how to do this.

2) Unzip the server.zip into apaches default location (/var/www/html/)
   if you now go to localhost in a webbrowser, you should see the site. But you will get an error
   when you go to 'See who is online', proceed to step 3.
   If you do not see the site at all (but maybe an error), fix that first.. chatgtp is your friend ;-)
   
3) goto localhost/install.php
   In this page, type your database root password. The page will create a new user and install the database.
   It also tries to create a dbCredent.php file to store the database credentials, If that step fails, you
   are asked to create that file manually!
