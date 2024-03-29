<?php

namespace FBS\Model;

class Material
{

    /**
     * @var string Identifies the material
     * @required
     */
    public $itemNumber = null;

    /**
     * @var Periodical Present if material is a periodical
     */
    public $periodical = null;

    /**
     * @var boolean True if material is available on-shelf, false if lent out
     * @required
     */
    public $available = null;

    /**
     * @var MaterialGroup Name og description of the material group that the material belongs to
     * @required
     */
    public $materialGroup = null;


}

