// OpenNI includes
#include <XnUSB.h>

// Standard includes
#include <stdio.h>
#include <time.h>

/**
 * Class to control Kinect's motor.
 */
class KinectMotor
{
public:
    KinectMotor();
    virtual ~KinectMotor();
    
    /**
     * Open device.
     * @return true if succeeded, false - overwise
     */
    bool Open();
    
    /**
     * Close device.
     */
    void Close();
    
    /**
     * Move motor up or down to specified angle value.
     * @param angle angle value
     * @return true if succeeded, false - overwise
     */
    bool Move(int angle);
    
private:
    XN_USB_DEV_HANDLE m_dev;
    bool m_isOpen;
};
