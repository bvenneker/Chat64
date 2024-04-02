<?php
//SPK 01

require_once('../dbCredent.php');

// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

if ($_SERVER["REQUEST_METHOD"] == "POST") {

    // Sanitize input data
    $regid = test_input($_POST["regid"]);
    $call = test_input($_POST["call"]);
    $sendername = test_input($_POST["sendername"]);
    $recipientname = test_input($_POST["recipientname"]);
    $recipientname = mb_convert_case($recipientname, MB_CASE_TITLE, "UTF-8");
    $message = test_input($_POST["message"]);

    // Decode base64 encoded message
    $message = base64_decode($message);

    // Escape special characters to prevent SQL injection
    $sendername = $conn->real_escape_string($sendername);
    $recipientname = $conn->real_escape_string($recipientname);
    $message = $conn->real_escape_string($message);

    // Prepare recipient ID
    $recipientID = "";

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

    // Insert message
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
