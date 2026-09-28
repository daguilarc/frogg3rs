// Classifies the visiting device from navigator.userAgent only -- no
// feature detection, since a desktop browser can support the same APIs
// (Screen Wake Lock, etc.) this classification is used to gate. Consumed by
// site-boot.mjs for the footer's download link (task 3.2) and the screen
// wake lock (task 3.3), both of which apply only to a phone.
export const ANDROID_PHONE = "android-phone";
export const IPHONE = "iphone";
export const UNCLASSIFIED = "unclassified";

export function classifyDevice() {
  const userAgent = navigator.userAgent;
  if (userAgent.includes("Android") && userAgent.includes("Mobile")) return ANDROID_PHONE;
  if (userAgent.includes("iPhone") || userAgent.includes("iPod")) return IPHONE;
  return UNCLASSIFIED;
}
