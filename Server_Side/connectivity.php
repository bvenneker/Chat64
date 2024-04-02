<?php
//Sven Pook v2


require_once('../dbCredent.php');

// Check if the request method is POST
if ($_SERVER["REQUEST_METHOD"] == "POST") {
    // Check database connection
    if (!$conn->connect_error) {
        // Connected successfully
        echo "Connected";
    } else {
        // Connection failed
        echo "Not connected";
    }
    
    // Close the database connection
    $conn->close();
}
?>