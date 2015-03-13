<?php

/**
 * @file
 * Test reservation flows.
 */

require_once "ProviderTestCase.php";
require_once 'includes/classes/FBS.php';
require_once dirname(__DIR__) . '/vendor/autoload.php';

if (!function_exists('ding_provider_build_entity_id')) {
  /**
   * Loan list calls this, mock it.
   */
  function ding_provider_build_entity_id($id, $agency = '') {
    return $id . ($agency ? ":" . $agency : '');
  }
                                                      }

/**
 * Some provider functions uses this.
 */
define('REQUEST_TIME', time());

/**
 * Test user provider functions.
 */
class FlowReservationTest extends ProviderTestCase {

  /**
   * Test reservation creation flow.
   *
   * Testgroup F1
   * Issue DDBFBS-30.
   *
   * @group flow
   */
  public function testCreation() {
    // // Define DING_RESERVATION_* constants..
    $this->requireDing('ding_reservation', 'ding_reservation.module');

    // Step 1
    // Login first.
    $json_responses = array(
      new Reply(
        array(
          // AuthenticatedPatron.
          'authenticated' => TRUE,
          'patron' => array(
            // Patron.
            'birthday' => '1946-03-19',
            'coAddress' => NULL,
            'address' => array(
              // Address
              'country' => 'Danmark',
              'city' => 'København',
              'street' => 'Alhambravej 1',
              'postalCode' => '1826',
            ),
            // ISIL of Vesterbro bibliotek
            'preferredPickupBranch' => '113',
            'onHold' => NULL,
            'patronId' => 234143,
            'recieveEmail' => TRUE,
            'blockStatus' => NULL,
            'recieveSms' => FALSE,
            'emailAddress' => 'onkel@danny.dk',
            'phoneNumber' => '80345210',
            'name' => 'Dan Turrell',
            'receivePostalMail' => FALSE,
            'defaultInterestPeriod' => 30,
            'resident' => TRUE,
          ),
        )
      )
    );

    $httpclient = $this->getHttpClient($json_responses);
    $fbs = fbs_service('1234', '', $httpclient, NULL, TRUE);
    $this->provider = 'user';
    $res = $this->providerInvoke('authenticate', '151019463013', '1234');

    $this->assertTrue($res['success']);
    $this->assertTrue(!empty($res['creds']['patronId']));

    $patron_id = $res['creds']['patronId'];
    $user = (object) array('creds' => $res['creds']);

    // Step 2
    // Now, get availability for something.
    // The record we'll try to reserve.
    $record_id = 'REC1';
    $json_responses = array(
      new Reply(
        array(
          array(
            'recordId' => $record_id,
            'available' => FALSE,
            'reservable' => TRUE,
          ),
        )
      ),
    );

    $this->replies($json_responses);
    $this->provider = 'availability';
    $res = $this->providerInvoke('items', array($record_id));

    $this->assertCount(1, $res);
    $this->assertArrayHasKey($record_id, $res);
    $this->assertTrue($res[$record_id]['reservable']);

    // Step 3
    // Looking at the material triggers a call to holdings, so we'll call that
    // too.
    $json_responses = array(
      new Reply(
        array(
          array(
            // HoldingsForBibliographicalRecord.
            'recordId' => $record_id,
            'reservable' => TRUE,
            'holdings' => array(
              array(
                // Holding.
                'materials' => array(
                  array(
                    // Material.
                    'itemNumber' => '1',
                    'available' => FALSE,
                  ),
                  array(
                    // Material.
                    'itemNumber' => '2',
                    'available' => TRUE,
                  ),
                  array(
                    // Material.
                    'itemNumber' => '3',
                    'available' => TRUE,
                  ),
                ),
                'branch' => array(
                  // AgencyBranch.
                  'branchId' => 'BRA1',
                  'title' => 'TBRA1',
                ),
                'department' => array(
                  // AgencyDepartment.
                  'departmentId' => 'DEP1',
                  'title' => 'TDEP1',
                ),
                'location' => array(
                  // AgencyLocation.
                  'locationId' => 'LOC1',
                  'title' => 'TLOC1',
                ),
                'sublocation' => array(
                  // AgencySublocation.
                  'sublocationId' => 'SUB1',
                  'title' => 'TSUB1',
                ),
              ),
            ),
          ),
        )
      ),
    );

    $this->replies($json_responses);
    $this->provider = 'availability';
    $res = $this->providerInvoke('holdings', array($record_id));

    $this->assertCount(1, $res);
    $this->assertArrayHasKey($record_id, $res);
    $this->assertTrue($res[$record_id]['reservable']);
    // Save total available for later.
    $total_available = 0;
    foreach ($res[$record_id]['holdings'] as $holding) {
      $total_available += $holding['available_count'];
    }

    // Step 4
    // Create reservation.
    $expected_expiry = date('Y-m-d', (REQUEST_TIME + ($user->creds['interest_period'] * 24 * 60 * 60)));
    $expected_reservation_date = date('Y-m-d', REQUEST_TIME);
    $json_responses = array(
      new Reply(
        array(
          // Array of...
          array(
            // ReservationDetails.
            'recordId' => $record_id,
            'pickupBranch' => 123,
            'expiryDate' => $expected_expiry,
            'reservationId' => 123,
            'pickupDeadline' => NULL,
            'dateOfReservation' => $expected_reservation_date,
            'state' => 'reserved',
            'numberInQueue' => 3,
          ),
        )
      ),
    );

    $this->replies($json_responses);
    $this->provider = 'reservation';
    $options = array(
      'interest_period' => $user->creds['interest_period'],
      'preferred_branch' => $user->creds['preferred_branch'],
    );
    $this->providerInvoke('create', $user, array($record_id), $options);
    // Create returns no result, so we can't check anything.

    // Step 5
    // Check that the material is on our reservation list.
    $json_responses = array(
      new Reply(
        array(
          // Array of...
          array(
            // ReservationDetails: MAT16
            'recordId' => $record_id,
            'pickupBranch' => $user->creds['preferred_branch'],
            'expiryDate' => $expected_expiry,
            'reservationId' => 16,
            'dateOfReservation' => $expected_reservation_date,
            'numberInQueue' => 1,
            'state' => 'reserved',
          ),
        )
      ),
    );

    $this->replies($json_responses);
    $this->provider = 'reservation';
    $options = array(
      'interest_period' => $user->creds['interest_period'],
      'preferred_branch' => $user->creds['preferred_branch'],
    );
    // We assume that our just created reservation wont be ready for pickup.
    $res = $this->providerInvoke('list', $user, DING_RESERVATION_NOT_READY);

    // Must be one..
    $this->assertGreaterThanOrEqual(1, count($res));
    $reservation = NULL;
    foreach ($res as $item) {
      if ($item['ding_entity_id'] == $record_id) {
        $reservation = $item;
        break;
      }
    }
    $this->assertNotNull($reservation);
    $this->assertEquals($expected_reservation_date, $reservation['created']);
    $this->assertEquals($user->creds['preferred_branch'], $reservation['pickup_branch_id']);
    $this->assertEquals($expected_expiry, $reservation['expiry']);

    // Step 4
    // Check that the holdings have updated.
    $json_responses = array(
      new Reply(
        array(
          array(
            // HoldingsForBibliographicalRecord.
            'recordId' => $record_id,
            'reservable' => TRUE,
            'holdings' => array(
              array(
                // Holding.
                'materials' => array(
                  array(
                    // Material.
                    'itemNumber' => '1',
                    'available' => FALSE,
                  ),
                  array(
                    // Material.
                    'itemNumber' => '2',
                    'available' => FALSE,
                  ),
                  array(
                    // Material.
                    'itemNumber' => '3',
                    'available' => TRUE,
                  ),
                ),
                'branch' => array(
                  // AgencyBranch.
                  'branchId' => 'BRA1',
                  'title' => 'TBRA1',
                ),
                'department' => array(
                  // AgencyDepartment.
                  'departmentId' => 'DEP1',
                  'title' => 'TDEP1',
                ),
                'location' => array(
                  // AgencyLocation.
                  'locationId' => 'LOC1',
                  'title' => 'TLOC1',
                ),
                'sublocation' => array(
                  // AgencySublocation.
                  'sublocationId' => 'SUB1',
                  'title' => 'TSUB1',
                ),
              ),
            ),
          ),
        )
      ),
    );

    $this->replies($json_responses);
    $this->provider = 'availability';
    $res = $this->providerInvoke('holdings', array($record_id));

    $this->assertCount(1, $res);
    $this->assertArrayHasKey($record_id, $res);
    // Check that available count has decreased.
    $available = 0;
    foreach ($res[$record_id]['holdings'] as $holding) {
      $available += $holding['available_count'];
    }
    $this->assertEquals($total_available - 1, $available);
  }
}