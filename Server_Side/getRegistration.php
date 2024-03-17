<?php
require_once('../dbCredent.php');
if ($_SERVER["REQUEST_METHOD"] == "POST") {
    $macaddress=test_input($_POST["macaddress"]);
    $regid=test_input($_POST["regid"]);
    $nickname=test_input($_POST["nickname"]);
    $version=test_input($_POST["version"]);
}

if (get_registration_status($macaddress,$regid)) {
  // registrtion is good now also check the nickname
  if (check_bad_nick_name($regid,$nickname)) {
    echo "r105";
    }
  else {
	  // update the (new) user name and version in the database
	  update_user($regid,$nickname,$version);
          echo "r200";
	  }
  }
  else{  echo "r104"; }

	
function get_registration_status($macaddress,$regid){
	global $conn;
	
	// select a row from the users table with the mac address and registration if from the request, where blocked =0
	// this should return one row.
   $sql="select * from users where blocked=0 and regid='".$regid."' and mac='".$macaddress."'"	;
    $result = $conn->query($sql);
    if ($result->num_rows == 1) {return true;}
	else{return false;}
	
}
	
function check_bad_nick_name($regid,$nickname){
	global $conn;
	
	// see if there is a user with a different registration id but with the same name (that is not allowed)
	$sql="select * from users where regid<>'".$regid."' and nickname='".$nickname."'"	;
     $result = $conn->query($sql);
    //echo "select * from users where regid<>'".$regid."' and nickname='".$nickname."'";
    if ($result->num_rows == 1) {return true;}
	else{return false;}	

}

function update_user($regid,$nickname,$version){
	global $conn;
	
	// Update the data
	$sql = "update users set nickname='".$nickname."',version='".$version."' where regid='".$regid."'";	
	$conn->query($sql);
	
}


function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}


?>
