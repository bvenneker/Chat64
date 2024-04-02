<?php
require_once('../dbCredent.php');
$system_regid="666666cacacacaffff";

if ($_SERVER["REQUEST_METHOD"] == "POST") {
  $sendername = test_input($_POST["sendername"]);
  $regid = test_input($_POST["regid"]);
  $lastmessage = test_input($_POST["lastmessage"]);
  $lastprivate = test_input($_POST["lastprivate"]);
  $type = test_input($_POST["type"]);
  $version = test_input($_POST["version"]);
  $eeprom =  test_input($_POST["rom"]);
  $lp = test_input($_POST["lp"]);
  if (isset($_POST["t"])) $timeoffset =  test_input($_POST["t"]);
  else $timeoffset = "+1";
  
} else  {
	exit(nothing);
}


// for backwards compatibility with esp version 3.1
if ( $type == "private" ) $type = 1;
if ( $type == "public" ) $type = 0;
// end of backwards compatibility

// Check connection
if ($conn->connect_error) {
  die("Connection failed: " . $conn->connect_error);
}

// =================================================================================================================
// If you are new, or if you have not logged on for a long time, You should not get pages and pages of messages.
// so if your lastmessage is very low, you will start at the 10th newest message 
$minlastmessage = $lastmessage;
$sql = "select min(rowid) as 'rowid' from  (SELECT * FROM messages WHERE recipient is null and sendername <> 'system' order by rowid desc limit 10) as table1";
$result = $conn->query($sql);
if ($row = $result->fetch_assoc()) {
	$minlastmessage = trim($row["rowid"]);
}
if ($lastmessage < $minlastmessage)  $lastmessage = $minlastmessage;

// the same for private messages
$minlastprivatemessage = $lastprivate;
$sql = "select min(rowid) as 'rowid' from  (SELECT * FROM messages WHERE recipient = '".$regid."' or (regid='".$regid."' and recipient is not null) and sendername <> 'system' order by rowid desc limit 10) as table1";
$result = $conn->query($sql);
if ($row = $result->fetch_assoc()) {
	$minlastprivatemessage = trim($row["rowid"]);
}
if ($lastprivate < $minlastprivatemessage)  $lastprivate = $minlastprivatemessage;



// =================================================================================================================
// see when I was last on line. If we have been away for >1 minutes, we post a message "<User> has joined the chat"
$sql="select lastseen from users where regid='".$regid."'";
$result = $conn->query($sql);

if ($row = $result->fetch_assoc()) {
      $lastseen=$row["lastseen"];
      if (( $lastseen==0 ) || ($lastseen <= (time()-60))) {
		$tmessage="[143][146]system: ".str_pad($sendername." joined the chat", 32, " ", STR_PAD_LEFT);
		  
		// delete old messages that say I joined the chat.
		$sql="delete from messages where message='".$tmessage."'"  ;
		$conn->query($sql);
		
        // post a new message "<myname> has joined the chat"
		$tmessage="[143][146]system: ".str_pad($sendername." joined the chat", 32, " ", STR_PAD_LEFT);
        $sql = "INSERT INTO messages (sendername, regid, message) VALUES ('system','" . $system_regid . "','" . $tmessage ."')";
        $conn->query($sql);
        
		// set the public message id back 10 messages so we can see what we missed or where we left off last time
		if ($lp != "1") $lastmessage = $minlastmessage;
      }
}

// =================================================================================================================
// update the last seen field in the users table so we can know this user is online
$sql="update users set lastseen=".time()."  where regid='".$regid."'";
$conn->query($sql);
// =================================================================================================================
// update the ESP32 version
if (strlen($version) > 0) {
	$sql="update users set version='".$version."' where regid='".$regid."'";
	$conn->query($sql);
}
// =================================================================================================================
// Update the eeprom version
if (strlen($eeprom) > 0) {
	$sql="update users set eeprom='".$eeprom."' where regid='".$regid."'";
	$conn->query($sql);
}
// =================================================================================================================
// get the last full page of messages at once
if ($lp == "1") {
	// lp==1 means get last full page of public messages.
	$sql="select * from messages where regid<>'".$system_regid."' and recipient is null order by rowid desc limit 25";	
	if ($result = $conn->query($sql)){
	
    $totlines=0;
	$targetID=0;
	$ids="";
    while($row = $result->fetch_assoc() and $totlines < 21){
		// create the header:
		$message="[151]".$row["timestamp"] . " " . $row["sendername"] .":";
		$message=str_pad($message,45,"@").$row["message"];
		// get the message length and replace the higher bytes (that contain color information)
        list($message,$len)=replace_higher_bytes($message);
	    // calculate the number of lines.
        $len = ceil($len/40); 
		$totlines += $len;

		$targetID = $row["rowid"];
		$ids = $row["rowid"] . '^^' . $ids;
	}
	 
	 foreach (explode("^^",$ids) as $i) {
	   outputMessage($i);
		 }
	 echo "                  ";

	}
	 
	exit(0);
}
	
// =================================================================================================================	
// select messages
  if ($type==0){
	  // public message
      $sql = "select m.rowid,m.timestamp, m.message,m.regid,u.nickname from messages m inner join users u on u.regid=m.regid where (m.regid<>'".$system_regid."' and recipient is null and m.rowid >" . $lastmessage . ") OR (m.regid='".$system_regid."' and m.timestamp > DATE_SUB(NOW(),INTERVAL 5 MINUTE) and m.rowid >" . $lastmessage . ")  order by m.rowid limit 1";
      $result = $conn->query($sql);
	  if($row = $result->fetch_assoc()){
			outputMessage($row["rowid"]);
	  } else{
			// no more public messages
			$pm=countPrivateMessages($regid,$lastprivate);
			echo '{"rowid":"'.$lastmessage.'","timestamp":"0","message":"0","nickname":"0","len":0,"pm":'.$pm.'}';
	  }
	  
	}
  else {
		// private message
        $sql = "select m.rowid,m.timestamp, m.message,m.regid,u.nickname,m.recipientname from messages m inner join users u on u.regid=m.regid where (recipient ='" . $regid  . "'  and m.rowid >" . $lastprivate . ") or (m.rowid >" . $lastprivate . " and m.regid='".$regid."' and recipient is not null)  order by m.rowid limit 1";
		$result = $conn->query($sql);
		if($row = $result->fetch_assoc()){
			outputMessage($row["rowid"]);
		} else {
			// no more private messages
			echo '{"rowid":"'.$lastprivate.'","timestamp":"0","message":"0","nickname":"0","len":0,"pm":0}';
		}       
	   }
echo"<br><br>";

 
// =================================================================================================================	  
      $conn->close();






// =================================================================================================================
// =================================================================================================================
// =================================================================================================================

function countPrivateMessages($regid,$lastprivate) {
	global $conn;
	$sql = "select m.rowid,m.timestamp, m.message,m.timestamp,u.nickname from messages m inner join users u on u.regid=m.regid where recipient ='" . $regid  . "'  and m.rowid >" . $lastprivate . " order by m.rowid";
        $result = $conn->query($sql);
       $pm = $result->num_rows ;
	return $pm;
}


function replace_higher_bytes($str){
  // this function replaces the [145] for byte value 145 en returns also the length of the message excluding those bytes.
  $pattern = "/\[\d\d\d\]/";
  preg_match_all($pattern, $str,$out);
    $c=0;
    foreach ($out[0] as $o) {
      $v=intval(substr($o,1,-1));
      $str = str_replace($o,chr($v),$str);
      $c++;
    }
  return array($str,strlen($str)-$c);
}


function test_input($data) {
    $data = trim($data);
    $data = stripslashes($data);
    $data = htmlspecialchars($data);
    return $data;
}

function trimMessage($message){
  $message=trim($message);

  // sometimes, for some reason, there are extra bytes at the end of the message.
  // They are formatted like this [nnn]. We need to delete those.
  
  while (preg_match("/^\[\d\d\d\]$/",substr($message,-5))) {
      $message=substr($message,0,-5);
      $message=trim($message);
  }
  return $message;
}

function cutAtName($message){
  // save the color byte for later
  if (substr($message,0,1)==="[" )  $colorBytes = substr($message,0,5);

  // find the first space or colon or semicolon
  while (1) {
    if (substr( $message, 0, 1 ) === " " or substr($message,0,1)===":" or substr($message,0,1)===";") break ;
      $message = substr($message,1);
      }

    $message = $colorBytes . substr($message,1);
    return $message;
}


function outputMessage($rowid){
	global $conn;
	global $lastprivate;
	global $regid;
	global $system_regid;
	global $timeoffset;
	
	
	$private=true;
	$sql = "select * from messages where rowid='".$rowid."'";
	 
	$result = $conn->query($sql);
	
	if ($row = $result->fetch_assoc()) {
		 
		$messageLine = trim($row["message"]);
        $senders_regid=$row["regid"];
	    $localtime = $row["timestamp"];
	    
	    
	    // Get the offset of this server from UTC	    
	    date_default_timezone_set("GMT");	    	// this is UTC timezone
		$dateTimeGMT = date('Y-m-d H:i:s'); 
		
		date_default_timezone_set("CET");		    // this is the locals servers timezone   
		$dateTimeLocal = date('Y-m-d H:i:s');  		 
		    
		$date1 = strtotime($dateTimeGMT);
		$date2 = strtotime($dateTimeLocal);
		$diff = $date2 - $date1;					// diff is the offset in seconds
		
		
		date_default_timezone_set("CET"); 			// restore our timezone
		
		// calculate UTC time from the value in the database
		$timeoffset = (int)$timeoffset; 
		$localtime = strtotime($row["timestamp"]) - $diff + ($timeoffset * 3600);
		$localtime =  date('Y-m-d H:i:s',$localtime);
		
		// system message
		if ($row["regid"] == $system_regid){
			// this is a system message	
			$message=$messageLine; 		
		} elseif (($row["recipient"] ==  $regid) or ($row["regid"]==$regid and is_null($row["recipient"])==false )) {
			// private message
			$messageLine = cutAtName($messageLine);
			// create the header text for a private message:
			$message="[151]" . $localtime . " " . $row["sendername"] . " @" . $row["recipientname"] . ":";
			$message=str_pad($message,45).$messageLine;
			$message=trimMessage($message);
			
		} else {
			// public message
			// create the header text for a public message:
			$message="[151]". $localtime . " " . $row["sendername"] .":";
			$message=str_pad($message,45).$messageLine;
			$message=trimMessage($message);
		}
		
		// get the message length and replace the higher bytes (that contain color information)
		list($message,$len)=replace_higher_bytes($message); 
	
		// calculate the number of lines.
		$len = ceil($len/40); 
		// build the json encoded string for output
		$message=base64_encode($message);
			
		$pm=countPrivateMessages($regid,$lastprivate);
		$out=array('rowid' => $row["rowid"] , 'timestamp'=> $localtime, 'message' => $message, 'regid' => $regid ,'nickname'=>$row["sendername"] ,'lines'=>$len,'pm'=>$pm  );
		 
 
		// output the row
		if (count($out) > 0 ) echo json_encode($out);
	}
	
}
?>
