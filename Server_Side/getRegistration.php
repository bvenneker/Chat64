<?php
//Sven Pook v2


require_once('../dbCredent.php');

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    // Validate and sanitize input data
    $macaddress = test_input($_POST["macaddress"]);
    $regid = test_input($_POST["regid"]);
    $nickname = test_input($_POST["nickname"]);
    $version = test_input($_POST["version"]);

    // Check if the registration status is valid
    if (get_registration_status($macaddress, $regid)) {
        // Check for a bad nickname
        if (check_bad_nick_name($regid, $nickname)) {
            echo "r105"; // Bad nickname
        } else {
            // Update user information
            if (update_user($regid, $nickname, $version)) {
                echo "r200"; // Success
            } else {
                echo "r500"; // Database error
            }
        }
    } else {
        echo "r104"; // Invalid registration
    }
}

// Function to check registration status
function get_registration_status($macaddress, $regid) {
    global $conn;

    // Prepare and execute the SQL query using a prepared statement
    $sql = "SELECT * FROM users WHERE blocked = 0 AND regid = ? AND mac = ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("ss", $regid, $macaddress);
    $stmt->execute();
    $result = $stmt->get_result();

    // Check if a single row is returned
    if ($result->num_rows == 1) {
        return true;
    } else {
        return false;
    }
}

// Function to check for bad nickname
function check_bad_nick_name($regid, $nickname) {
    global $conn;

    // Prepare and execute the SQL query using a prepared statement
    $sql = "SELECT * FROM users WHERE regid <> ? AND nickname = ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("ss", $regid, $nickname);
    $stmt->execute();
    $result = $stmt->get_result();

    // Check if a single row is returned
    if ($result->num_rows == 1) {
        return true;
    } else {
        return false;
    }
}

// Function to update user information
function update_user($regid, $nickname, $version) {
    global $conn;

    // Prepare and execute the SQL query using a prepared statement
    $sql = "UPDATE users SET nickname = ?, version = ? WHERE regid = ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("sss", $nickname, $version, $regid);
    if ($stmt->execute()) {
        return true;
    } else {
        return false;
    }
}

// Function to sanitize input data
function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>
