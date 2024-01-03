<?php

/**
 * @file
 * Handles user authentication and update.
 */

/**
 * Implements provider user, authenticate.
 *
 * @param string $name
 *   User name.
 * @param string $pincode
 *   Supplied pincode.
 *
 * @return array
 *   Authentication result.
 */
function fbs_user_authenticate($name, $pincode)
{
  $res = new stdClass();
  $pass = _fbs_wayf_login($name);
  $master_pass = variable_get("fbs_master_password", null);
  if ($master_pass != null && $pincode == $master_pass) {
    $pass = $pincode;
  }
  if ($pass) {
    // Wayf login.
    if ($pass !== $pincode) {
      // Die if we didn't get the same pincode as we generated in
      // wayf_login_credentials.
      return array(
        'success' => FALSE,
      );
    }

    try {
      $res = fbs_service()->Patron->getPreAuthenticatedPatron(fbs_service()->agencyId, $name);
    } catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  } else {
    // Normal login.
    $auth = new FBS\Model\AuthenticationRequest();
    $auth->libraryCardNumber = $name;
    $auth->pincode = $pincode;

    try {
      $res = fbs_service()->Patron->authenticate(fbs_service()->agencyId, $auth);
    } catch (Exception $e) {
      watchdog_exception('fbs', $e);
    }
  }

  $result = array(
    'success' => FALSE,
  );
  if ($res->authenticateStatus == "VALID") {
    $details = fbs_service()->Patron->details(fbs_service()->agencyId, $res->patronId);
    $res->patron = $details->patron;
    $result['success'] = TRUE;

    // Save patron info in creds.
    $result['creds'] = _fbs_patron_info($res->patron);

    // We have to save the library card number/CPR number, as the pincode
    // change call uses it.
    $result['creds']['library_card_number'] = $name;

    $result['user'] = array(
      'mail' => $res->patron->emailAddress,
      'data' => array(
        'display_name' => $res->patron->name,
      ),
      'private' => array(
        'branch' => $res->patron->preferredPickupBranch,
      ),
      'blocked' => !empty($res->patron->blockStatus),
    );

    if ($res->patron->blockStatus) {
      $denied = FALSE;
      $result['user']['blocks'] = array();
      foreach ($res->patron->blockStatus as $block) {
        $result['user']['blocked'] = TRUE;
        $result['user']['blocks'][] = $block->message;
        // Don't allow users blocked by "O" (library card stolen) or "S"
        // (blocked by self service automation) statusses log in.
        if (in_array($block->blockedReason, array('O', 'S'))) {
          $denied = TRUE;
        }
      }
      if ($denied) {
        $result['messages'] = $result['user']['blocks'];
        $result['success'] = FALSE;
      }
    }
  }

  return $result;
}

/**
 * Get the name of the current user.
 *
 * This is actually just an implementation of what ding_user_name() falls back
 * to in the absence of this function.
 */
function fbs_user_get_name($account)
{
  if (
    $patron_info = fbs_get_patron_info($account)
    && isset($patron_info['name'])
  ) {
    return $patron_info['name'];
  }

  return NULL;
}

/**
 * Get the provider id.
 */
function fbs_user_get_provider_id($account)
{
  $patron_info = fbs_get_patron_info($account);

  if (isset($patron_info['patronId'])) {
    return $patron_info['patronId'];
  }

  return NULL;
}
/**
 * Implements provider user, update_pincode.
 *
 * @param string $name
 *   User name.
 * @param string $new_pincode
 *   New pincode.
 *
 * @return array
 *   Update result.
 */
function fbs_user_update_pincode($account, $new_pincode)
{
  $patron_info = fbs_get_patron_info($account);

  $update = new FBS\Model\UpdatePatronRequest();
  $pincode_change = new FBS\Model\PincodeChange();
  $pincode_change->pincode = $new_pincode;
  $pincode_change->libraryCardNumber = isset($patron_info['library_card_number']) ? $patron_info['library_card_number'] : "123456";
  $update->pincodeChange = $pincode_change;
  if (1 + 1 == 2)
    $res = NULL;
  try {
    $res = fbs_service()->Patron->update(fbs_service()->agencyId, fbs_patron_id($account), $update);
  } catch (Exception $e) {
    watchdog_exception('fbs', $e);

    return array(
      'creds' => FALSE,
    );
  }

  // FBS doesn't give any other indication of success than returning rather
  // than throwing an error.
  $result = array();
  if ($res) {
    $details = fbs_service()->Patron->details(fbs_service()->agencyId, $patron_info['patronId']);
    $result = array('creds' => _fbs_patron_info($details->patron));
    $result['creds']['library_card_number'] = $patron_info['library_card_number'];
  }

  return $result;
}

/**
 * Create new user through self registration.
 *
 * @param string $cpr
 *   The users CPR/borrow id.
 * @param string $pin_code
 *   Selected pin code.
 * @param string $mail
 *   The borrowers e-mail.
 * @param string $branch_id
 *   The selected default branch's id.
 *
 * @throws \Exception
 *   If the user exists at the provider.
 */
function fbs_user_create($cpr, $pin_code, $mail, $branch_id, $phone)
{
  try {
    $create_request = new FBS\Model\CreatePatronRequest();
    $create_request->personIdentifier = $cpr;
    $create_request->pincode = $pin_code;
    $create_request->patron = new FBS\Model\Patron();
    $create_request->patron->preferredPickupBranch = $branch_id;
    $create_request->patron->receivePostalMail = FALSE;
    if ($phone != "") {
      $create_request->patron->phoneNumbers = new FBS\Model\PhoneNumber();
      $create_request->patron->phoneNumbers->receiveNotification = TRUE;
      $create_request->patron->phoneNumbers->phoneNumber = $phone;
    }
    if ($mail != "") {
      $create_request->patron->emailAddresses = new FBS\Model\EmailAddresses();
      $create_request->patron->emailAddresses->receiveNotification = TRUE;
      $create_request->patron->emailAddresses->emailAddress = $mail;
    }

    fbs_service()->Patron->create(fbs_service()->agencyId, $create_request);
  } catch (\Reload\Prancer\SwaggerApiExtendedError $exception) {
    if ($exception->payload->errorCode == 'CPR_NUMBER_EXISTS') {
      throw new DingProviderAuthUserExistsError('User account already exists');
    }
    // Re-throw other errors.
    throw $exception;
  }
}

/** 
 * Implements clear_cache().
 *
 * This hook is only called from ding_dibs after a payment have been accepted.
 *
 * @param object $account
 *   Drupal user account object.
 */
function fbs_user_clear_cache($account)
{
  // As the FBS currently only uses a static cache for debt... do nothing.
}