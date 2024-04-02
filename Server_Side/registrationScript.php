<?php
//Sven Pook v2


ini_set('display_errors', 0);
ini_set('display_startup_errors', 0);
error_reporting(E_ERROR);

require_once('../dbCredent.php');

// Check database connection
if ($conn->connect_error) {
    die("Unable to connect to database: " . $conn->connect_error);
}

if ($_SERVER["REQUEST_METHOD"] == "POST" && isset($_POST['submit'])) {
    $full_name = isset($_POST['full_name']) ? test_input($_POST['full_name']) : '';
    $email = isset($_POST['email']) ? test_input($_POST['email']) : '';
    $mac = isset($_POST['mac']) ? test_input($_POST['mac']) : '';

    $validName = "/^[a-zA-Z ]*$/";
    $validEmail = "/^\w+([\.-]?\w+)*@\w+([\.-]?\w+)*(\.\w{2,3})+$/";
    $validMac = "/^[a-f0-9]{10}$/";

    $fnameErr = validateField($full_name, $validName);
    $emailErr = validateField($email, $validEmail);
    $macErr = validateField($mac, $validMac);

    if ($fnameErr === true && $emailErr === true && $macErr === true) {
        $fullName = legal_input($full_name);
        $email = legal_input($email);
        $mac = legal_input($mac);
        $register = register($fullName, $email, $mac);
        echo $register;
    } else {
        $set_fullName = $full_name;
        $set_email = $email;
        $set_mac = $mac;
    }
}

function validateField($field, $pattern)
{
    if (empty($field)) {
        return "Required!";
    } elseif (!preg_match($pattern, $field)) {
        return "Bad Format!";
    } else {
        return true;
    }
}

function legal_input($value)
{
    $value = trim($value);
    $value = stripslashes($value);
    $value = htmlspecialchars($value);
    return $value;
}

function register($fullName, $email, $mac)
{
    global $conn;

    // Generate registration code
    $data = strtolower($mac . $email);
    $regcode = hash("adler32", $data, false) . hash("crc32", $data, false);

    // Prepare and execute the SQL query using a prepared statement
    $sql = "DELETE FROM users WHERE mac = ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("s", $mac);
    $stmt->execute();

    $sql = "INSERT INTO users (fullname, email, regid, mac, sendmail) VALUES (?, ?, ?, ?, 1)";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("ssss", $fullName, $email, $regcode, $mac);
    
    if ($stmt->execute()) {
        return "Please check your email for the registration code";
    } else {
        return "Error: Registration failed";
    }
}

function test_input($data)
{
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>
