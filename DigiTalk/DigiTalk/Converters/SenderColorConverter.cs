// Конвертер для цвета шрифта отправителя сообщения
// Если сообщение в чате принадлежит текущему пользователю, конвертер возвращает розовую кисть, иначе - черную

using Avalonia.Data.Converters;
using Avalonia.Media;
using System;
using System.Globalization;

namespace DigiTalk.Converters
{
    public class SenderColorConverter : IValueConverter
    {
        public object Convert(object value, Type targetType, object parameter, CultureInfo culture)
        {
            if (value is bool isOwnMessage)
            {
                return isOwnMessage ? new SolidColorBrush(Color.Parse("#a37ddb")) : new SolidColorBrush(Color.Parse("#f59eff"));
            }
            return Brushes.Black;
        }

        public object ConvertBack(object value, Type targetType, object parameter, CultureInfo culture)
        {
            throw new NotImplementedException();
        }
    }
}