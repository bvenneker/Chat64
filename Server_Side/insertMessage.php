<?php
//Sven Pook v2
//may 7, 2024 BV added retry count


require_once('../dbCredent.php');

// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

if ($_SERVER["REQUEST_METHOD"] == "POST") {

    // Sanitize input data
    $regid = test_input($_POST["regid"]);
    if (isset($_POST["call"])) $call = test_input($_POST["call"]);
    
    if ($call == "heartbeat") {
        // Update last seen field for the user
        $sql = "UPDATE users SET lastseen = " . time() . " WHERE regid = '$regid'";
        if ($conn->query($sql) === TRUE) {
            echo "0";
        } else {
            echo "1"; // Error updating last seen
        }
        $conn->close();
        exit();
    }
    
    $retryCount=0;
    if (isset($_POST["retryCount"])) $retryCount = test_input($_POST["retryCount"]); 
    if (isset($_POST["sendername"])) $sendername = test_input($_POST["sendername"]);    
    if (isset($_POST["recipientname"])) { 
		$recipientname = test_input($_POST["recipientname"]);
		$recipientname = mb_convert_case($recipientname, MB_CASE_TITLE, "UTF-8");
	}
    $message = test_input($_POST["message"]);

    // Decode base64 encoded message
    $message = base64_decode($message);
	
    // delete the last char if it is not printable
    if (ctype_print(substr($message, -1))==False){
        $message = substr_replace($message, '', -1);
    }
		
    // Escape special characters to prevent SQL injection
    $sendername = $conn->real_escape_string($sendername);
    $recipientname = $conn->real_escape_string($recipientname);
    $message = $conn->real_escape_string($message);

    // Prepare recipient ID
    $recipientID = "";

    // Insert message
    // if retryCount > 0, check if the message is allready in the database, not older that 6 seconds
    if ($retryCount>0){
		$sql="select * from messages where timestamp > DATE_SUB(NOW(),INTERVAL 6 SECOND) and regid='$regid' and message='$message'";
		$result = $conn->query($sql);
        if ($result->num_rows > 0) {
            echo "0"; // Message sent successfully
            exit();
        }
    }
		
		
    if (empty($recipientname)) {
        $sql = "INSERT INTO messages (sendername, regid, message) VALUES ('$sendername', '$regid', '$message')";
    } else {
        $sql = "SELECT regid FROM users WHERE nickname = '$recipientname'";
        $result = $conn->query($sql);
        if ($result->num_rows > 0) {
            $row = $result->fetch_assoc();
            $recipientID = $row["regid"];
        }
        if ($recipientID != "") {
            $sql = "INSERT INTO messages (sendername, regid, recipientname, recipient, message) VALUES ('$sendername', '$regid', '$recipientname', '$recipientID', '$message')";
        } else {
            echo "3"; // Recipient not found
            $conn->close();
            exit();
        }
    }

    if ($conn->query($sql) === TRUE) {
        echo "0"; // Message sent successfully
    } else {
        echo "1"; // Error inserting message
    }

    $conn->close();
}

function test_input($data)
{
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>
