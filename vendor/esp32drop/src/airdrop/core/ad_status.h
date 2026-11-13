/* ad_status.h -- what the activity needs to know about the AirDrop layer.
 * Dependency-free C, so the same code compiles into the firmware and into a host test. */
#pragma once
#include <stdbool.h>
#include <stdint.h>

struct AdStatus {
  bool upload_pending;       /* an /Upload body is being received */
  uint32_t rx_bytes;         /* compressed bytes of the current upload received so far */
  uint32_t file_err;         /* requests/uploads that failed (cumulative) */
  uint32_t limit_rejections; /* requests/uploads rejected at the size limit (cumulative) */
};
