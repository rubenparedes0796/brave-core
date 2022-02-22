/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import * as React from 'react'

// Feature-specific components
import { Content, Title, Paragraph } from '../../components'

// Utils
import { getLocale } from '../../../common/locale'

// Images
import { WelcomeShieldsImage } from '../../components/images'

interface Props {
  index: number
  currentScreen: number
}

// Hack inline style until a Checkbox or Toggle component is available.
const hackStyleDiv = {
  color: '#FFFFFF',
  fontFamily: 'Muli,sans-serif',
  fontSize: 22,
  textAlign: 'center' as 'center',
  //display: 'block',
  WebkitFontSmoothing: 'antialiased',
};
// Override margins on screen-select bullet buttons.
const hackStyleInput = {
  margin: 12,
};

function contentWrap(index: number, currentScreen: number, inner) {
  return (
    <Content
      zIndex={index}
      active={index === currentScreen}
      screenPosition={'1' + (index + 1) + '0%'}
      isPrevious={index <= currentScreen}
    >
      <WelcomeShieldsImage />
      <Title>{getLocale('privacyTitle')}</Title>
      {inner}
    </Content>
  )
}

export default class ShieldsBox extends React.PureComponent<Props> {
  render () {
    const text = getLocale('p3aDesc').split('$1')
    const opt_in = true

    const { index, currentScreen } = this.props

    let desc
    if (opt_in) { desc = [(
        <Paragraph>
          { getLocale('shieldsDesc') }
        </Paragraph>
        ), (
        <div style={hackStyleDiv}>
          <label>
            <input
              type='checkbox'
              style={hackStyleInput}
            />
            { getLocale('p3aCheckbox') }
          </label>
        </div>
        ), (
        <Paragraph>
          {text[0]}
          <a
            href='https://brave.com/p3a'
            target='_blank'
            rel='noopener noreferrer'
          >
            {text[1]}
          </a>
          {text[2]}
          <a
            href='chrome://settings/privacy'
            target='_blank'
            rel='noopener noreferrer'
          >
            {text[3]}
          </a>
          {text[4]}
        </Paragraph>
      )]
    } else { desc = (
      <Paragraph>
        { getLocale('shieldsDesc') }
        {text[0]}
-          <a
-            href='https://brave.com/p3a'
-            target='_blank'
-            rel='noopener noreferrer'
-          >
-            {text[1]}
-          </a>
-          {text[2]}
-          <a
-            href='brave://settings/privacy'
-            target='_blank'
-            rel='noopener noreferrer'
-          >
-            {text[3]}
-          </a>
-          {text[4]}
      </Paragraph>
    )}

    return contentWrap(index, currentScreen, desc)
  }
}
