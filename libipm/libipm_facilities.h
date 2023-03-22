/**
 * Copyright (C) 2022 Matt Burt, all xrdp contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @file    libipm/libipm_facilities.h
 * @brief   Facilities numbers for facilities built on top of libipm
 */

#if !defined(LIBIPM__FACILITIES_H)
#define LIBIPM__FACILITIES_H

/**
 * Facilities layered on top of libipm (16 bits)
 */
enum libipm_facility
{
    LIBIPM_FAC_SCP = 1, /**< SCP - Sesman Control Protocol */
    LIBIPM_FAC_EICP, /**< EICP - Executive Initialization Control Protocol */
    LIBIPM_FAC_ERCP, /**< ERCP - Executive Run-time Control Protocol */

    LIBIPM_FAC_TEST = 65535 /**< Used for unit testing */
};

#endif /* LIBIPM_FACILITIES_H */
