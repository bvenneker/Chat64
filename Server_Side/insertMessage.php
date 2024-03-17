<?php

require_once('../dbCredent.php');
// Check connection
if ($conn->connect_error) {
	die("Connection failed: " . $conn->connect_error);
	exit();
}

if ($_SERVER["REQUEST_METHOD"] == "POST") {

	$regid = test_input($_POST["regid"]);	
	$call = test_input($_POST["call"]);
	$sendername = test_input($_POST["sendername"]);
	$recipientname = test_input($_POST["recipientname"]);
	$recipientname = mb_convert_case($recipientname, MB_CASE_TITLE, "UTF-8");
	$message = test_input($_POST["message"]);
	$message = base64_decode($message);
	$message = str_replace("'","''",$message);
	$recipientID = "";

	if ($call=="heartbeat"){		
		// this is a heartbeat update.
		// update the last seen field in the users table so we can know this user is online
		$sql="update users set lastseen=".time()."  where regid='".$regid."'";
		$conn->query($sql);
		$conn->close();
		echo "0";
		exit();
	}

		
	if ($recipientname == ""){
	   $sql = "INSERT INTO messages (sendername, regid, message) VALUES ('" . $sendername . "','" . $regid . "','" . $message ."')";
           if ($conn->query($sql) === TRUE) {
             echo "0";
           }
	   else {
             echo "1"; //Insert failed
	   }
           //$conn->close();
        }
	else {
           $mysql = 'SELECT * FROM users WHERE nickname="'.$recipientname.'"';
	   $result = $conn->query($mysql); 
	   $count = mysqli_num_rows($result);	
           if($count > 0){
              $reg=mysqli_fetch_array($result);
	      $recipientID = $reg['regid'];	
	   }
           if ($recipientID != ""){
	      $sql = "INSERT INTO messages (sendername, regid, recipientname, recipient, message) VALUES ('" . $sendername . "','" . $regid . "','" . $recipientname . "','" . $recipientID . "','" . $message ."')";
              if ($conn->query($sql) === TRUE) {
				echo "0";        
              }
              else {
                 echo "1"; //insert failed
              }
              
              
              
           }
	   else {
	      echo "3";   // Recipient not found, wrongly spelled? message not inserted
              }
        }
	
        $conn->close();
}

function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>
