<?php

/**
 * @file
 * Handles user debts.
 */

/**
 * Implements provider debt, list.
 *
 * @param object $account
 *   User to get debt listing for.
 * @param bool $reset
 *   If TRUE cache is reset (default: FALSE).
 * @param int $patron_id
 *   Id of the patron at the provider.
 * @param bool $include_paid
 *   Default to FALSE, if TRUE already paid are also returned.
 *
 * @return array
 *   Debt information.
 */
function fbs_debt_list($account, $reset = FALSE, $patron_id = NULL, $include_paid = FALSE) {
  $results = &drupal_static(__FUNCTION__, NULL);

  if (is_null($results) || $reset) {
    // Check if ding_session_cache is available.
    if (module_exists('ding_session_cache') && !$reset) {
      $results = ding_session_cache_get('fbs', 'debt', NULL);
    }

    // If both static cache and session cache failed, try reloading information
    // from FBS.
    if (is_null($results) || $reset) {
      $results = array();
      $res = array();

      try {
        // Gets ERROR OC changes
        //$patron_id = is_null($patron_id) ? fbs_patron_id($account) : $patron_id;
        $include_paid = FALSE === $include_paid ? 'false' : 'true';
        $res = fbs_service()->Payment->getFees(fbs_service()->agencyId, fbs_patron_id($account), $include_paid, 'true');
      }
      catch (Exception $e) {
        watchdog_exception('fbs', $e);
      }

      $ids = array();
      foreach ($res as $fee) {
        foreach ($fee->materials as $material) {
          $ids[$material->recordId] = $material->recordId;
        }
      }

      // Translate record ids and preload entities.
      if ($ids) {
        //OC changes not needed - $ids = ding_provider_build_entity_id($ids);
        $entities = array();
        // Since ding_entitye_load_multiple() returns array keyed by local tid, we
        // make e new array keyed by data well PiD.
        foreach (ding_entity_load_multiple(array_filter($ids)) as $key => $entity) {
          $entity_id = explode(':', $entity->ding_entity_id);
          $entities[$entity_id[1]] = $entity;
        }
      }

      foreach ($res as $fee) {
        /* @var \FBS\Model\Fee $fee */
        $id = $fee->feeId;

        $data = array(
          'date' => $fee->creationDate,
          'duedate' => $fee->dueDate,
          'display_name' => $fee->reasonMessage,
          // FBS only provides the amount left to be paid on each fee, so this
          // will only reflect that.
          'amount' => $fee->amount,
          // And the only thing we can do here is set it to zero, as the original
          // amount isn't available.
          'amount_paid' => 0,
          'paid_date' => $fee->paidDate,
          'type' => fbs_debt_translate_debt_type($fee->type),
          'payable' => $fee->payableByClient,
        );

        // If the has associated materials; collect some information about them and
        // try to build a more useful title.
        $materials = array();
        foreach ($fee->materials as $material) {
          $ding_entity_id = $ids[$material->recordId];
          if (isset($entities[$ding_entity_id])) {
            $entity = $entities[$ding_entity_id];
            // Create title that links to the object.
            $uri = entity_uri('ting_object', $entity);

            $title = t('@material_title (@material_number)', array(
              '@material_title' => $entity->getTitle(),
              '@material_number' => $material->materialItemNumber,
            ));
            $materials[] = l($title, $uri['path'], $uri['options']);
          }
          else {
            // TODO: Maybe we can do better before falling back to this. E.g. like
            // trying to use a pseudo entity like ding_reservation.
            $materials[] = t('Unknown material (@material_number)', array(
              '@material_number' => $material->materialItemNumber,
            ));
          }
        }

        if (!empty($materials)) {
          // Apply som inline span-elements, so the different materials in the
          // display_name can be easily styled.
          $data['display_name'] = '<span>' . implode('</span><span>', $materials) . '</span>';

          if (count($materials) == 1) {
            $data['material_type'] = !empty($entity) ? $entity->type : t('Unknown type');
          }
          else {
            $data['material_type'] = t('Multiple materials');
          }
        }

        $results[$id] = new DingProviderDebt($id, $data);
      }

      // Store the loans into ding session cache.
      if (module_exists('ding_session_cache')) {
        ding_session_cache_set('fbs', 'debt', $results);
      }
    }
  }

  return $results;
}

/**
 * Translate FBS debt types into a human readable string in english.
 *
 * @param string $debt_type
 *   The FBS debt type.
 *
 * @return string
 *   English translation of the debt type if found.
 */
function fbs_debt_translate_debt_type($debt_type) {
  switch ($debt_type) {
    case 'compensation':
      $str = t('Kompensation', [], ['context' => 'fbs']);
      break;

    case 'fee':
      $str = t('Gebyr', [], ['context' => 'fbs']);
      break;

    default:
      $str = t('Andre', [], ['context' => 'fbs']);
  }

  return $str;
}

/**
 * Implements provider debt, payment_received.
 *
 * @param object $account
 *   User to register payment for.
 * @param array $debt_ids
 *   Depts paid for.
 * @param string $order_id
 *   Transaction id.
 * @param int $patron_id
 *   Id of the patron at the provider.
 *
 * @return bool
 *   Success or not.
 */
function fbs_debt_payment_received($account, $debt_ids = array(), $order_id = NULL, $patron_id = NULL) {
  $payment_order = new FBS\Model\PaymentOrder();
  $payment_order->orderId = $order_id;
  $payment_order->feeIds = array();

  foreach ($debt_ids as $debt_id) {
    $payment_order->feeIds[] = $debt_id;
  }
  if (cache_get(session_name() . '-' . $account->uid, 'cache_ding_user_credentials')) {
    $patron_id = is_null($patron_id) ? fbs_patron_id($account) : $patron_id;
  } else {
    $roles = array_values($GLOBALS['user']->roles);
    // Log out user who are authenticated but has no other roles (i.e. ding user)
    if (user_is_logged_in() && sizeof($roles) == 1 && $roles[0] == 'authenticated user') {
      user_module_invoke('logout', $account, 'user');
      session_destroy();
      $GLOBALS['user'] = drupal_anonymous_user();
      watchdog('ding_debt_easy', 'The payment callback has been called with a session timeout/logout. Order (%order_id). The order have feeIds: %feeIds', [
        '%order_id' => $payment_order->orderId,
        '%feeIds' => $payment_order->feeIds,
      ], WATCHDOG_CRITICAL);
      drupal_set_message(t('Du har været inaktiv i lang tid og er derfor blevet loggede ud af sikkerhedshensyn, venligst kontakt bibliotekets regningsafdeling med ordrenummer: %order_id da din betaling ikke er korrekt registreret', [
        '%order_id' => $payment_order->orderId,
      ]), 'error');
      _ding_debt_easy_update_status_local($payment_order->orderId, DING_DEBT_EASY_STATUS_FAILED);
      drupal_goto(DING_DEBT_USER_DEBT_PAGE);
    }
  }

  try {
    $res = fbs_service()->Payment->payFees(fbs_service()->agencyId, $patron_id, $payment_order);
  }
  catch (Exception $e) {
    watchdog_exception('fbs', $e);
    return FALSE;
  }

  foreach ($res as $confirmation) {
    if ($confirmation->paymentStatus !== 'paymentRegistered') {
      $variables = array(
        '@patron_id' => $patron_id,
        '@order_id' => $order_id,
        '@fee_id' => implode(', ', $debt_ids),
      );
      watchdog('fbs', 'Could not register payment for patron @patron_id, order id @order_id, fee id @fee_id', $variables, WATCHDOG_ERROR);
      return FALSE;
    }
  }

  return TRUE;
}
