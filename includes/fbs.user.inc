<?php

/**
 * @file
 * Handles user authentication and update.
 */

/**
 * Implements provider user, authenticate.
 */
function fbs_user_authenticate($name, $pincode) {
  $auth = new FBS\Model\AuthenticationRequest();
  $auth->libraryCardNumber = $name;
  $auth->pincode = $pincode;

  $res = fbs_service()->Patron->authenticate(fbs_service()->agencyId, $auth);
  $result = array(
    'success' => $res->authenticated,
  );
  if ($res->authenticated) {
    // Much ding code relies on creds existing for logged in users. So we
    // provide a dummy value for it to store.
    $result['creds'] = TRUE;

    $result['user'] = array(
      'mail' => $res->patron->emailAddress,
      'data' => array(
        'display_name' => $res->patron->name,
      ),
      'private' => array(
        'branch' => $res->patron->preferredPickupBranch,
      ),
    );
  }

  return $result;
}

/**
 * Implements provider user, update_pincode.
 */
function fbs_user_update_pincode($account, $new_pincode) {
  $update = new FBS\Model\UpdatePatronRequest();
  $update->pincodeChange = $new_pincode;
  $res = fbs_service()->Patron->update(fbs_service()->agencyId, fbs_patron_id($account), $update);
  // FBS doesn't give any other indication of success than returning rather
  // than throwing an error.
  return array('creds' => TRUE);
}