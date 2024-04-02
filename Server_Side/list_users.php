<?php
require_once('../dbCredent.php');
//Sven Pook v2


// Set default values
$page = 0;
$call = "";
$listversion = "1";
$regid = "a2990acd5efb562d";

// Handle POST request
if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $regid = test_input($_POST["regid"]);
    if (isset($_POST["page"])) $page = intval($_POST["page"]);
    if (isset($_POST["call"])) $call = test_input($_POST["call"]);
    if (isset($_POST["version"])) $listversion = test_input($_POST["version"]);
}

// Check connection
if ($conn->connect_error) {
    die("Connection failed: " . $conn->connect_error);
}

$pagesize = 20;
$offset = $page * $pagesize;

if ($call == "list") {
    echo get_list_of_users_in_text();
} else {
    echo get_list_of_users_in_petsci($offset, $pagesize);
}

//Functions
function get_list_of_users_in_text()
{
    global $conn;
    $list = "";
    $sql = "SELECT nickname FROM users WHERE nickname IS NOT NULL AND blocked = 0";
    $result = $conn->query($sql);
    while ($row = $result->fetch_assoc()) {
        $list .= $row['nickname'] . ';';
    }
    return strtolower($list);
}

function get_list_of_users_in_petsci($offset, $pagesize)
{
    global $conn, $regid;
    $sql = "SELECT nickname, lastseen, regid FROM users WHERE nickname IS NOT NULL AND blocked = 0 ORDER BY nickname LIMIT ? OFFSET ?";
    $stmt = $conn->prepare($sql);
    $stmt->bind_param("ii", $pagesize, $offset);
    $stmt->execute();
    $result = $stmt->get_result();

    $screen = "";
    while ($row = $result->fetch_assoc()) {
        $color = chr(156); // Gray

        if (time() - $row['lastseen'] < 30) $color = chr(149); // Green for online users

        if ($regid == $row['regid']) $color = chr(149); // Green for the current user

        $screen .= $color . str_pad($row['nickname'], 10);
    }
    if ($screen == "") $screen = " ";
    return $screen;
}

function test_input($data)
{
    $data = trim($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>


