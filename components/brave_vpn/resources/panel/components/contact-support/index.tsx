import * as React from 'react'
import { Button } from 'brave-ui'

import { getLocale } from '../../../../../common/locale'
//import { useSelector } from '../../state/hooks'
import * as S from './style'
import { CaratStrongLeftIcon } from 'brave-ui/components/icons'

interface Props {
  closeContactSupport: React.MouseEventHandler<HTMLButtonElement>
}

function ContactSupport (props: Props) {
  const handleSubmit = () => {
  }

  return (
    <S.Box>
      <S.PanelContent>
        <S.PanelHeader>
          <S.BackButton
            type='button'
            onClick={props.closeContactSupport}
            aria-label='Close support form'
          >
            <i><CaratStrongLeftIcon /></i>
            <span>{getLocale('braveVpnContactSupport')}</span>
          </S.BackButton>
        </S.PanelHeader>
        <S.List>
          <li>VPN hostname:</li>
          <li>App Version:</li>
          <li>
            Issue:
            <select name="issue" id="contact-support-issue">
              <option value="">Please choose a reason</option>
              <option value="cant-connect">Cannot connect to the VPN (Other error)</option>
              <option value="no-internet">No internet when connected</option>
              <option value="slow">Slow connection</option>
              <option value="website">Website doesn't work</option>
              <option value="other">Other</option>
            </select>
          </li>
          <li>App Version:</li>
        </S.List>
        <Button
          level='primary'
          type='accent'
          brand='rewards'
          //text={getLocale('braveVpnEditPaymentMethod')}
          text='Submit'
          onClick={handleSubmit}
        />
      </S.PanelContent>
    </S.Box>
  )
}

export default ContactSupport
