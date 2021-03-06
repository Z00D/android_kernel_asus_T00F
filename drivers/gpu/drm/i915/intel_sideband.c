/*
 * Copyright © 2013 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "i915_drv.h"
#include "intel_drv.h"

/* IOSF sideband */
static int vlv_sideband_rw(struct drm_i915_private *dev_priv, u32 devfn,
			   u32 port, u32 opcode, u32 addr, u32 *val)
{
	u32 cmd, be = 0xf, bar = 0;
	bool is_read;
	devfn = 0x00;

	is_read = (opcode == PUNIT_OPCODE_REG_READ ||
			opcode == DPIO_OPCODE_REG_READ);

	if (port == IOSF_PORT_PMC)
		if (is_read)
			opcode = 0x0;
		else
			opcode = 0x1;
	else
		if (is_read)
			opcode = 0x6;
		else
			opcode = 0x7;

	cmd = (devfn << IOSF_DEVFN_SHIFT) | (opcode << IOSF_OPCODE_SHIFT) |
		(port << IOSF_PORT_SHIFT) | (be << IOSF_BYTE_ENABLES_SHIFT) |
		(bar << IOSF_BAR_SHIFT);

	mutex_lock(&dev_priv->new_dpio_lock);
//	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for_atomic((I915_READ(VLV_IOSF_DOORBELL_REQ)
				& IOSF_SB_BUSY) == 0, 5)) {
		DRM_DEBUG_DRIVER("IOSF sideband idle wait (%s) timed out\n",
				 is_read ? "read" : "write");
		mutex_unlock(&dev_priv->new_dpio_lock);
		return -EAGAIN;
	}

	I915_WRITE(VLV_IOSF_ADDR, addr);
	if (!is_read)
		I915_WRITE(VLV_IOSF_DATA, *val);
	I915_WRITE(VLV_IOSF_DOORBELL_REQ, cmd);

	if (wait_for_atomic((I915_READ(VLV_IOSF_DOORBELL_REQ)
				& IOSF_SB_BUSY) == 0, 5)) {
		DRM_DEBUG_DRIVER("IOSF sideband finish wait (%s) timed out\n",
				 is_read ? "read" : "write");
		mutex_unlock(&dev_priv->new_dpio_lock);
		return -ETIMEDOUT;
	}

	if (is_read)
		*val = I915_READ(VLV_IOSF_DATA);
	I915_WRITE(VLV_IOSF_DATA, 0);

	mutex_unlock(&dev_priv->new_dpio_lock);
	return 0;
}

static int vlv_sideband_rw32_bits(struct drm_i915_private *dev_priv, u32 devfn,
			   u32 port, u32 opcode, u32 addr, u32 *val, u32 mask)
{
	u32 tmp;
	int status;

	status = vlv_sideband_rw(dev_priv, DPIO_DEVFN, port,
					DPIO_OPCODE_REG_READ, addr, &tmp);
	if (status != 0)
		return status;
	tmp = tmp & ~mask;
	*val = *val & mask;
	tmp = *val | tmp;

	return vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_PMC,
				DPIO_OPCODE_REG_WRITE, addr, &tmp);

}

u32 vlv_punit_read(struct drm_i915_private *dev_priv, u8 addr)
{
	u32 val = 0;

//	mutex_lock(&dev_priv->dpio_lock);
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_PUNIT,
			PUNIT_OPCODE_REG_READ, addr, &val);
//	mutex_unlock(&dev_priv->dpio_lock);

	return val;
}

void vlv_punit_write(struct drm_i915_private *dev_priv, u8 addr, u32 val)
{

//	mutex_lock(&dev_priv->dpio_lock);
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_PUNIT,
			PUNIT_OPCODE_REG_WRITE, addr, &val);
//	mutex_unlock(&dev_priv->dpio_lock);
}
/*Changed type of parameter addr from u8 to u16. Can be made to u32 too*/
u32 vlv_nc_read(struct drm_i915_private *dev_priv, u16 addr)
{
	u32 val = 0;

//	mutex_lock(&dev_priv->dpio_lock);
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_NC,
			PUNIT_OPCODE_REG_READ, addr, &val);
//	mutex_unlock(&dev_priv->dpio_lock);

	return val;
}

u32 vlv_gpio_nc_read(struct drm_i915_private *dev_priv, u32 reg)
{
	u32 val = 0;
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_GPIO_NC,
			PUNIT_OPCODE_REG_READ, reg, &val);
	return val;
}

void vlv_gpio_nc_write(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_GPIO_NC,
			PUNIT_OPCODE_REG_WRITE, reg, &val);
}

u32 vlv_cck_read(struct drm_i915_private *dev_priv, u32 reg)
{
	u32 val = 0;
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_CCK,
			PUNIT_OPCODE_REG_READ, reg, &val);
	return val;
}

void vlv_cck_write(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_CCK,
			PUNIT_OPCODE_REG_WRITE, reg, &val);
}

void vlv_cck_write_bits(struct drm_i915_private *dev_priv, u32 reg, u32 val, u32 mask)
{
	vlv_sideband_rw32_bits(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_CCK,
				PUNIT_OPCODE_REG_WRITE, reg, &val, mask);
}

u32 vlv_ccu_read(struct drm_i915_private *dev_priv, u32 reg)
{
	u32 val = 0;
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_CCU,
			PUNIT_OPCODE_REG_READ, reg, &val);
	return val;
}

void vlv_ccu_write(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_CCU,
			PUNIT_OPCODE_REG_WRITE, reg, &val);
}

u32 vlv_gps_core_read(struct drm_i915_private *dev_priv, u32 reg)
{
	u32 val = 0;
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_GPS_CORE,
			PUNIT_OPCODE_REG_READ, reg, &val);
	return val;
}

void vlv_gps_core_write(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	vlv_sideband_rw(dev_priv, PCI_DEVFN(2, 0), IOSF_PORT_GPS_CORE,
			PUNIT_OPCODE_REG_WRITE, reg, &val);
}

u32 vlv_dpio_read(struct drm_i915_private *dev_priv, int reg)
{
	u32 val = 0;

	vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_DPIO,
			DPIO_OPCODE_REG_READ, reg, &val);

	return val;
}

void vlv_dpio_write(struct drm_i915_private *dev_priv, int reg, u32 val)
{
	vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_DPIO,
			DPIO_OPCODE_REG_WRITE, reg, &val);
}

/* SBI access */
u32 intel_sbi_read(struct drm_i915_private *dev_priv, u16 reg,
		   enum intel_sbi_destination destination)
{
	u32 value = 0;
	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for((I915_READ(SBI_CTL_STAT) & SBI_BUSY) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to become ready\n");
		return 0;
	}

	I915_WRITE(SBI_ADDR, (reg << 16));

	if (destination == SBI_ICLK)
		value = SBI_CTL_DEST_ICLK | SBI_CTL_OP_CRRD;
	else
		value = SBI_CTL_DEST_MPHY | SBI_CTL_OP_IORD;
	I915_WRITE(SBI_CTL_STAT, value | SBI_BUSY);

	if (wait_for((I915_READ(SBI_CTL_STAT) & (SBI_BUSY | SBI_RESPONSE_FAIL)) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to complete read transaction\n");
		return 0;
	}

	return I915_READ(SBI_DATA);
}

void intel_sbi_write(struct drm_i915_private *dev_priv, u16 reg, u32 value,
		     enum intel_sbi_destination destination)
{
	u32 tmp;

	WARN_ON(!mutex_is_locked(&dev_priv->dpio_lock));

	if (wait_for((I915_READ(SBI_CTL_STAT) & SBI_BUSY) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to become ready\n");
		return;
	}

	I915_WRITE(SBI_ADDR, (reg << 16));
	I915_WRITE(SBI_DATA, value);

	if (destination == SBI_ICLK)
		tmp = SBI_CTL_DEST_ICLK | SBI_CTL_OP_CRWR;
	else
		tmp = SBI_CTL_DEST_MPHY | SBI_CTL_OP_IOWR;
	I915_WRITE(SBI_CTL_STAT, SBI_BUSY | tmp);

	if (wait_for((I915_READ(SBI_CTL_STAT) & (SBI_BUSY | SBI_RESPONSE_FAIL)) == 0,
				100)) {
		DRM_ERROR("timeout waiting for SBI to complete write transaction\n");
		return;
	}
}

int intel_pmc_read32(struct drm_i915_private *dev_priv, u32 reg, u32 *val)
{

	return vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_PMC,
				DPIO_OPCODE_REG_READ, reg, val);
}

int intel_pmc_write32(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	return vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_PMC,
				DPIO_OPCODE_REG_WRITE, reg, &val);
}

int intel_pmc_write32_bits(struct drm_i915_private *dev_priv, u32 reg,
				u32 val, u32 mask)
{
	return vlv_sideband_rw32_bits(dev_priv, DPIO_DEVFN, IOSF_PORT_PMC,
				DPIO_OPCODE_REG_WRITE, reg, &val, mask);
}

int intel_flisdsi_read32(struct drm_i915_private *dev_priv, u32 reg, u32 *val)
{
	return vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_FLISDSI, 
				DPIO_OPCODE_REG_READ, reg, val);
}

int intel_flisdsi_write32(struct drm_i915_private *dev_priv, u32 reg, u32 val)
{
	return vlv_sideband_rw(dev_priv, DPIO_DEVFN, IOSF_PORT_FLISDSI, 
				DPIO_OPCODE_REG_WRITE, reg, &val);
}

int intel_flisdsi_write32_bits(struct drm_i915_private *dev_priv, u32 reg,
				u32 val, u32 mask)
{
	return vlv_sideband_rw32_bits(dev_priv, DPIO_DEVFN, IOSF_PORT_FLISDSI, 
				DPIO_OPCODE_REG_WRITE, reg, &val, mask);
}
