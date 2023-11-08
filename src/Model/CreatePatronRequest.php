<?php

namespace FBS\Model;

class CreatePatronRequest
{

    /**
     * @var string 
     * @required
     */
    public $personIdentifier = null;

    /**
     * @var string 
     * @required
     */
    public $pincode = null;

    /**
     * @var PatronSettings 
     * @required
     */
    public $patron = null;


}

