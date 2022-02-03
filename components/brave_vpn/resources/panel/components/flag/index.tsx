import * as React from 'react'
import { hasFlag } from 'country-flag-icons'
import * as Flags from 'country-flag-icons/react/3x2'

interface Props {
  countryCode: string
}

function Flag (props: Props) {
  if (hasFlag(props.countryCode)) {
    const Icon = Flags[props.countryCode]
    return <Icon />
  }

  return null
}

export default Flag
