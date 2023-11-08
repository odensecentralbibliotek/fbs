<?php

namespace FBS\Model;

class PatronSettings
{

    /**
     * @var string Language in which the patron prefers the communication with the library to take place If left empty default library language will be used.
     */
    public $preferredLanguage = null;

    /**
     * @var array Notification protocols that the patron want to receive notification on. SMS and EMAIL are not included.
     */
    public $notificationProtocols = null;

    /**
     * @var EmailAddresses Existing email addresses are overwritten with these values If left empty existing email addresses are deleted.
     */
    public $emailAddresses = null;

    /**
     * @var string ISIL-number of preferred pickup branch
     * @required
     */
    public $preferredPickupBranch = null;

    /**
     * @var Period If not set then the patron is not on hold
     */
    public $onHold = null;

    /**
     * @var boolean
     * @required
     */
    public $guardianVisibility = null;

    /**
     * @var boolean
     * @required
     */
    public $receivePostalMail = null;

    /**
     * @var array A list of interests of the patron.
     */
    public $interests = null;

    /**
     * @var PhoneNumber Required if patron should receive SMS notifications
     */
    public $phoneNumbers = null;

}

