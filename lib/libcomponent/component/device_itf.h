#ifndef __DEVICE_ITF_H__
#define __DEVICE_ITF_H__


/** 
 * IDeviceControl is a common control-plane interface for most HW
 * devices.  It does not inherit from Component::IBase since it is
 * normal inhereted by an explicit interface such as IBlockDevice.
 * 
 */
class IDeviceControl
{
public:
  /** 
   * Initialize device. Called once.
   * 
   * @param instance Device instance identifier counting from 0.
   * 
   * @return S_OK on successfully device intialization. E_NOT_FOUND if device instance cannot be found.
   */
  virtual status_t init_device(unsigned instance, config_t config = NULL) = 0;

  /** 
   * Shutdown the device and free resources.  After this is called no other methods can be called subsequently.
   * 
   * 
   * @return S_OK on success. E_INVAL if device is already shutdown. E_FAIL on shutdown error.
   */
  virtual status_t shutdown_device() = 0;

  /** 
   * Get hold of initialize Device class (see xdk/lib/libexo/exo/device.h)
   * 
   * 
   * @return Pointer to Device instance or NULL if init_device has not been called yet.
   */
  virtual Exokernel::Device * get_device() = 0;

};

#endif // __DEVICE_ITF_H__
