<?php

require_once('../dbCredent.php');


if ($_SERVER["REQUEST_METHOD"] == "POST") {
        
        if (!$conn->connect_error) {
		echo "Connected";
        } else {
 		echo "Not connected";
	}
        $conn->close();
}
?>
