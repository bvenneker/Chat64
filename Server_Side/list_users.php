<?php
require_once('../dbCredent.php');

$page=0;
$call="";

if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $regid=test_input($_POST["regid"]);
    if (isset($_POST["page"])) $page=test_input($_POST["page"]);
    if (isset($_POST["call"])) $call=test_input($_POST["call"]);
    if (isset($_POST["version"]))
		$call=$listversion=test_input($_POST["version"]);
	else
		$listversion="1";

} else {
	$regid="a2990acd5efb562d";
    //$call="list";
    $listversion="2";
    $page=1;
	}

// Check connection
if ($conn->connect_error) {
        die("Connection failed: " . $conn->connect_error);
}



$pagesize = 20;
$offset = $page * $pagesize;
if ($call=="list"){
  $r=get_list_of_users_in_text();
  echo $r;
} else {
  $r = get_list_of_users_in_petsci($offset,$pagesize);
  echo $r;
}


function get_list_of_users_in_text(){
  global $conn;
  $list="";
  $sql="select nickname from users where nickname is not null and blocked=0";
  $result = $conn->query($sql);
  while ($row=mysqli_fetch_assoc($result)) {
    $list .= $row['nickname'].';';
  }
  return strtolower($list);
}


function get_list_of_users_in_petsci($offset,$pagesize){
    global $conn;
    global $regid;
    $sql="select nickname ,lastseen, regid  from users where nickname is not null and blocked=0 order by nickname LIMIT " . $pagesize . " OFFSET " . $offset;
    
    $result = $conn->query($sql);
    
	$emparray = array();
	$screen="";
	while ($row=mysqli_fetch_assoc($result)) {
		// nu de kleur bepalen aan de hand van de last seen timestamp
		$color=chr(156); // 156 = gray //  149=green
		
		// if the users lastseen timestamp was updated less than 30 seconds ago, we asume they are online
		if ( time() - $row['lastseen'] < 30 ) $color=chr(149);
		
		// your own username should always be green
		if ($regid == $row['regid']) $color=chr(149);
		
		$screen.= $color . str_pad($row['nickname'],10);
		}
	if ($screen=="" )$screen=" ";
    return $screen;
}

function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}
?>

