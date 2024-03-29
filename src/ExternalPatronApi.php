<?php

namespace FBS;

use Reload\Prancer\SwaggerApi;
use FBS\Model;

class ExternalPatronApi extends SwaggerApi
{

    /* Get patron details */
    public function details($agencyid, $patronid)
    {
        $request = $this->newRequest("GET", "/external/{agencyid}/patrons/{patronid}/v4");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("path", "patronid", $patronid);

        $request->defineResponse(200, "", '\\FBS\\Model\\DetailsPatron');
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);

        return $request->execute();
    }


    /**
     * Create a new patron.
     *
     *
     *  When a patron doesn't have a patron account in the library system, but logs in using a trusted authentication
     *  source
     *  (e.g NemId), the patron account can be created using this service. Name and address will be automatically
     *  fetched from
     *  CPR-Registry, and cannot be supplied by the client. If the CPR-Registry is not authorized to provide
     *  information about the patron, then the name and address will not be set.
     *  
     *  If a patron is blocked the reason is available as a code:
     *  
     *      - 'O': library card stolen
     *      - 'U': exclusion
     *      - 'F': extended exclusion
     *      - 'S': blocked by self service automaton
     *      - 'W': self created at website
     *  
     *  The codes are informational, and can be used for looking up end user messages by the client system. However,
     *  the list is subject to change at any time, so any unexpected values should be interpreted as 'other reason'.
     *
     * @param string $agencyid ISIL of the agency (e.g. DK-761500)
     * @param CreatePatronRequest $createPatronRequest the patron to be created
     * @return AuthenticatedPatron
     */
    public function create($agencyid, Model\CreatePatronRequest $createPatronRequest)
    {
        $request = $this->newRequest("POST", "/external/{agencyid}/patrons/v9");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("body", "createPatronRequest", $createPatronRequest);

        $request->defineResponse(200, "", '\\FBS\\Model\\AuthenticatedPatron');
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);

        return $request->execute();
    }

    /**
     * Authenticates a patron and returns the patron details.
     *
     *
     *  The returned patron details includes a patronId that has to be used by all subsequent
     *  service calls made on behalf of that patron.
     *  
     *  If a patron is blocked the reason is available as a code:
     *  
     *      - 'O': library card stolen
     *      - 'U': exclusion
     *      - 'F': extended exclusion
     *      - 'S': blocked by self service automaton
     *      - 'W': self created at website
     *  
     *  The codes are informational, and can be used for looking up end user messages by the client system. However,
     *  the list is subject to change at any time, so any unexpected values should be interpreted as 'other reason'.
     *
     * @param string $agencyid ISIL of the agency (e.g. DK-761500)
     * @param AuthenticationRequest $authenticationRequest credentials for patron to be authenticated
     * @return AuthenticatedPatron
     */
    public function authenticate($agencyid, Model\AuthenticationRequest $authenticationRequest)
    {
        $request = $this->newRequest("POST", "/external/{agencyid}/patrons/authenticate/v9");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("body", "authenticationRequest", $authenticationRequest);

        $request->defineResponse(200, "", '\\FBS\\Model\\AuthenticatedPatron');
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);

        return $request->execute();
    }

    /**
     * Returns the patron details of a patron that the client has pre-authenticated using a third party.
     *
     *
     *  The returned patron details includes a patronId that has to be used by all subsequent
     *  service calls made on behalf of that patron.
     *  
     *  If a patron is blocked the reason is available as a code:
     *  
     *      - 'O': library card stolen
     *      - 'U': exclusion
     *      - 'F': extended exclusion
     *      - 'S': blocked by self service automaton
     *      - 'W': self created at website
     *  
     *  The codes are informational, and can be used for looking up end user messages by the client system. However,
     *  the list is subject to change at any time, so any unexpected values should be interpreted as 'other reason'.
     *
     * @param string $agencyid ISIL of the agency (e.g. DK-761500)
     * @param string $cprNumber CPR-number of the patron
     * @return AuthenticatedPatron
     */
    public function getPreAuthenticatedPatron($agencyid, $cprNumber)
    {
        $request = $this->newRequest("POST", "/external/{agencyid}/patrons/preauthenticated/v10");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("body", "cprNumber", $cprNumber);

        $request->defineResponse(200, "", '\\FBS\\Model\\AuthenticatedPatron');
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);

        return $request->execute();
    }

    /**
     * Returns the patron details of a patron that the client has pre-authenticated using UNIC.
     *
     *
     *  The returned patron details includes a patronId that has to be used by all subsequent
     *  service calls made on behalf of that patron.
     *  
     *  If a patron is blocked the reason is available as a code:
     *  
     *      - 'O': library card stolen
     *      - 'U': exclusion
     *      - 'F': extended exclusion
     *      - 'S': blocked by self service automaton
     *      - 'W': self created at website
     *  
     *  The codes are informational, and can be used for looking up end user messages by the client system. However,
     *  the list is subject to change at any time, so any unexpected values should be interpreted as 'other reason'.
     *
     * @param string $agencyid ISIL of the agency (e.g. DK-761500)
     * @param string $unicUsername UNIC username of the patron
     * @return AuthenticatedPatron
     */
    public function getPreAuthenticatedPatronFromUNIClogin($agencyid, $unicUsername)
    {
        $request = $this->newRequest("POST", "/external/{agencyid}/patrons/preauthenticated/unic/v6");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("body", "unicUsername", $unicUsername);

        $request->defineResponse(200, "", '\\FBS\\Model\\AuthenticatedPatron');
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);

        return $request->execute();
    }

    /**
     * Update information about the patron.
     *
     *
     *  The name and address cannot be supplied by the client. If the CPR-Registry is not authorized to provide
     *  information about the patron, then the name and address will not be updated.
     *  It is possible to either update just the pincode, update just some patron settings, or update both.
     *  
     *  If a patron is blocked the reason is available as a code:
     *  
     *      - 'O': library card stolen
     *      - 'U': exclusion
     *      - 'F': extended exclusion
     *      - 'S': blocked by self service automaton
     *      - 'W': self created at website
     *  
     *  The codes are informational, and can be used for looking up end user messages by the client system. However,
     *  the list is subject to change at any time, so any unexpected values should be interpreted as 'other reason'.
     *
     * @param string $agencyid ISIL of the agency (e.g. DK-761500)
     * @param integer $patronid the patron to be updated
     * @param UpdatePatronRequest $updatePatron updated information about the patron
     * @return AuthenticatedPatron
     */
    public function update($agencyid, $patronid, Model\UpdatePatronRequest $updatePatron)
    {
        $request = $this->newRequest("PUT", "/external/{agencyid}/patrons/{patronid}/v8");
        $request->addParameter("path", "agencyid", $agencyid);
        $request->addParameter("path", "patronid", $patronid);
        $request->addParameter("body", "updatePatron", $updatePatron);

        $request->defineResponse(204, "Update OK");
        $request->defineResponse("400", 'bad request', '\\FBS\\Model\\RestException');
        $request->defineResponse("401", 'client unauthorized', null);
        $request->defineResponse("404", 'patron not found', null);

        return $request->execute();
    }


}

